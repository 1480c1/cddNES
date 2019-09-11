#include "ppu.h"

#include <stdlib.h>
#include <string.h>

static uint32_t PALETTE[64] = {
	0xFF6A6D6A, 0xFF801300, 0xFF8A001E, 0xFF7A0039, 0xFF560055, 0xFF18005A, 0xFF00104F, 0xFF001C3D,
	0xFF003225, 0xFF003D00, 0xFF004000, 0xFF243900, 0xFF552E00, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFFB9BCB9, 0xFFC75018, 0xFFE3304B, 0xFFD62273, 0xFFA91F95, 0xFF5C289D, 0xFF003798, 0xFF004C7F,
	0xFF00645E, 0xFF007722, 0xFF027E02, 0xFF457600, 0xFF8A6E00, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFFFFFFFF, 0xFFFFA668, 0xFFFF9C8C, 0xFFFF86B5, 0xFFFD75D9, 0xFFB977E3, 0xFF688DE5, 0xFF299DD4,
	0xFF0CAFB3, 0xFF11C27B, 0xFF47CA55, 0xFF81CB46, 0xFFC5C147, 0xFF4A4D4A, 0xFF000000, 0xFF000000,
	0xFFFFFFFF, 0xFFFFEACC, 0xFFFFDEDD, 0xFFFFDAEC, 0xFFFED7F8, 0xFFF5D6FC, 0xFFCFDBFD, 0xFFB5E7F9,
	0xFFAAF0F1, 0xFFA9FADA, 0xFFBCFFC9, 0xFFD7FBC3, 0xFFF6F6C4, 0xFFBEC1BE, 0xFF000000, 0xFF000000,
};

static float EMPHASIS[8][3] = {
	{1.00f, 1.00f, 1.00f}, // 000 Black
	{1.00f, 0.85f, 0.85f}, // 001 Red
	{0.85f, 1.00f, 0.85f}, // 010 Green
	{0.85f, 0.85f, 0.70f}, // 011 Yellow
	{0.85f, 0.85f, 1.00f}, // 100 Blue
	{0.85f, 0.70f, 0.85f}, // 101 Magenta
	{0.70f, 0.85f, 0.85f}, // 110 Cyan
	{0.70f, 0.70f, 0.70f}, // 111 White
};

static uint8_t POWER_UP_PALETTE[32] = {
	0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
	0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08,
};

//PPUSTATUS $2002
enum ppu_status_flags {
	FLAG_STATUS_O = 0x20, //sprite overflow
	FLAG_STATUS_S = 0x40, //sprite 0 hit
	FLAG_STATUS_V = 0x80, //vblank
};

#define GET_CX(reg)       ((reg) & 0x001F)
#define GET_CY(reg)       (((reg) & 0x03E0) >> 5)
#define GET_NT(reg)       (((reg) & 0x0C00) >> 10)
#define GET_FY(reg)       (((reg) & 0x7000) >> 12)

#define SET_CX(reg, cx)   ((reg) = ((reg) & 0x7FE0) | ((uint16_t) ((cx) & 0x1F)))
#define SET_CY(reg, cy)   ((reg) = ((reg) & 0x7C1F) | ((uint16_t) ((cy) & 0x1F) << 5))
#define SET_NT_H(reg, nt) ((reg) = ((reg) & 0x7BFF) | ((uint16_t) ((nt) & 0x01) << 10))
#define SET_NT_V(reg, nt) ((reg) = ((reg) & 0x77FF) | ((uint16_t) ((nt) & 0x02) << 10))
#define SET_NT(reg, nt)   ((reg) = ((reg) & 0x73FF) | ((uint16_t) ((nt) & 0x03) << 10))
#define SET_FY(reg, fy)   ((reg) = ((reg) & 0x0FFF) | ((uint16_t) ((fy) & 0x07) << 12))

#define SET_H(reg, h)     ((reg) = ((reg) & 0x00FF) | ((uint16_t) ((h) & 0x3F) << 8))
#define SET_L(reg, l)     ((reg) = ((reg) & 0x7F00) | ((uint16_t) (l)))

#define FLIP_NT_H(reg)    ((reg) ^= 0x0400)
#define FLIP_NT_V(reg)    ((reg) ^= 0x0800)

struct sprite {
	uint16_t addr;
	uint8_t low_tile;
	uint8_t id;
};

struct spr {
	uint8_t color;
	bool priority;
	bool sprite0;
};

struct ppu {
	uint32_t pixels[256 * 240];
	uint32_t palettes[8][64];
	uint32_t *palette;

	uint8_t palette_ram[32];
	uint8_t oam[256];
	uint8_t soam[8][4];

	struct {
		bool nmi_enabled;
		uint8_t incr;
		uint8_t sprite_h;
		uint8_t nt;
		uint16_t bg_table;
		uint16_t sprite_table;
	} CTRL;

	struct {
		uint8_t grayscale;
		bool show_bg;
		bool show_sprites;
		bool clip_bg;
		bool clip_sprites;
		bool rendering;
	} MASK;

	uint8_t STATUS;
	uint8_t OAMADDR;

	uint16_t bus_v;  //the current ppu address bus -- important for A12 toggling signal for MMC3
	uint16_t v;      //current vram address
	uint16_t t;      //temporary vram address holds state between STATUS and/or write latch
	uint8_t x;       //fine x scroll
	bool w;          //write latch
	bool f;          //even/odd frame flag

	uint8_t bgl;
	uint8_t bgh;
	uint8_t nt;
	uint8_t attr;
	uint8_t bg[272];

	uint8_t oam_n;
	uint8_t soam_n;
	uint8_t eval_step;
	bool overflow;
	struct sprite sprites[8];
	struct spr spr[256];

	uint8_t open_bus;
	uint8_t read_buffer;
	uint8_t decay_high2;
	uint8_t decay_low5;
	bool supress_nmi;

	uint16_t scanline;
	uint16_t dot;
	bool palette_write;
};


/*** VRAM ***/

static void ppu_scroll_h(struct ppu *ppu);
static void ppu_scroll_v(struct ppu *ppu);

static bool ppu_visible(struct ppu *ppu)
{
	return ppu->MASK.rendering && (ppu->scanline <= 239 || ppu->scanline == 261);
}

static void ppu_set_bus_v(struct ppu *ppu, struct cart *cart, uint16_t v)
{
	if (!(ppu->bus_v & 0x1000) && (v & 0x1000))
		cart_ppu_a12_toggle(cart);

	ppu->bus_v = v;
}

static void ppu_set_v(struct ppu *ppu, struct cart *cart, uint16_t v, bool glitch)
{
	// https://wiki.nesdev.com/w/index.php/PPU_scrolling#.242007_reads_and_writes
	if (glitch && ppu_visible(ppu)) {
		ppu_scroll_h(ppu);
		ppu_scroll_v(ppu);

	} else {
		ppu->v = v;

		if (!ppu_visible(ppu))
			ppu_set_bus_v(ppu, cart, v);
	}
}

static uint8_t ppu_read_palette(struct ppu *ppu, uint16_t addr)
{
	addr &= (addr % 4 == 0) ? 0x0F : 0x1F;

	return ppu->palette_ram[addr] & ppu->MASK.grayscale;
}

static uint8_t ppu_read_vram(struct ppu *ppu, struct cart *cart, uint16_t addr, enum mem type, bool nt)
{
	if (addr < 0x3F00) {
		if (addr < 0x2000)
			ppu_set_bus_v(ppu, cart, addr);

		return cart_chr_read(cart, addr, type, nt);

	} else {
		return ppu_read_palette(ppu, addr);
	}
}

static void ppu_write_vram(struct ppu *ppu, struct cart *cart, uint16_t addr, uint8_t v)
{
	if (addr < 0x3F00) {
		if (addr < 0x2000)
			ppu_set_bus_v(ppu, cart, addr);

		cart_chr_write(cart, addr, v);

	} else {
		addr &= (addr % 4 == 0) ? 0x0F : 0x1F;

		ppu->palette_ram[addr] = v;
		ppu->palette_write = true;
	}
}


/*** READ & WRITE ***/

uint8_t ppu_read(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr)
{
	uint8_t v = ppu->open_bus;

	switch (addr) {
		case 0x2002:
			ppu->decay_high2 = 0;

			// https://wiki.nesdev.com/w/index.php/PPU_frame_timing#VBL_Flag_Timing
			if (ppu->scanline == 241 && ppu->dot == 1) {
				ppu->supress_nmi = true;

			} else if (ppu->scanline == 241 && ppu->dot > 1 && ppu->dot < 4) {
				cpu_nmi(cpu, false);
			}

			v = ppu->open_bus = (ppu->open_bus & 0x1F) | ppu->STATUS;
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_V);
			ppu->w = false;
			break;

		case 0x2004:
			ppu->decay_high2 = ppu->decay_low5 = 0;

			if (ppu_visible(ppu)) {
				int32_t pos = ppu->dot - 257;
				int32_t n = pos / 8;
				int32_t m = (pos % 8 > 3) ? 3 : pos % 8;

				v = ppu->open_bus =
					(pos >= 0 && n < 8) ? ppu->soam[n][m] :
					(ppu->dot < 65 || (ppu->soam_n == 8 && (ppu->dot & 0x01) == 0)) ? ppu->soam[0][0] :
					ppu->oam[ppu->OAMADDR];

			} else {
				v = ppu->open_bus = ppu->oam[ppu->OAMADDR];
			}

			break;

		case 0x2007: {
			uint16_t waddr = ppu->v & 0x3FFF;
			ppu->decay_high2 = ppu->decay_low5 = 0;

			//buffered read from CHR
			if (waddr < 0x3F00) {
				v = ppu->open_bus = ppu->read_buffer;
				ppu->read_buffer = ppu_read_vram(ppu, cart, waddr, ROM_DATA, false);

			} else {
				//read buffer gets ciram byte
				ppu->read_buffer = ppu_read_vram(ppu, cart, waddr - 0x1000, ROM_DATA, false);

				//upper 2 bits get preserved from decay value
				v = (ppu->open_bus & 0xC0) | (ppu_read_vram(ppu, cart, waddr, ROM_DATA, false) & 0x3F);
			}

			ppu_set_v(ppu, cart, ppu->v + ppu->CTRL.incr, true);
			break;
		}
	}

	return v;
}

void ppu_write(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr, uint8_t v)
{
	ppu->decay_high2 = ppu->decay_low5 = 0;
	ppu->open_bus = v;

	switch (addr) {
		case 0x2000:
			if ((v & 0x80) && GET_FLAG(ppu->STATUS, FLAG_STATUS_V) && !ppu->CTRL.nmi_enabled)
				cpu_nmi(cpu, true);

			if (!(v & 0x80) && ppu->scanline == 241 && ppu->dot < 5)
				cpu_nmi(cpu, false);

			ppu->CTRL.nt = v & 0x03;
			ppu->CTRL.incr = (v & 0x04) ? 32 : 1;
			ppu->CTRL.sprite_table = (v & 0x08) ? 0x1000 : 0;
			ppu->CTRL.bg_table = (v & 0x10) ? 0x1000 : 0;
			ppu->CTRL.sprite_h = (v & 0x20) ? 16 : 8;
			ppu->CTRL.nmi_enabled = v & 0x80;

			SET_NT(ppu->t, ppu->CTRL.nt);
			break;

		case 0x2001:
			ppu->MASK.grayscale = (v & 0x01) ? 0x30 : 0x3F;
			ppu->MASK.clip_bg = v & 0x02;
			ppu->MASK.clip_sprites = v & 0x04;
			ppu->MASK.show_bg = v & 0x08;
			ppu->MASK.show_sprites = v & 0x10;
			ppu->MASK.rendering = ppu->MASK.show_bg || ppu->MASK.show_sprites;

			ppu->palette = ppu->palettes[(v & 0xE0) >> 5];
			break;

		case 0x2003:
			ppu->OAMADDR = v;
			break;

		case 0x2004:
			// https://wiki.nesdev.com/w/index.php/PPU_registers#OAM_data_.28.242004.29_.3C.3E_read.2Fwrite

			if (!ppu_visible(ppu)) {
				if ((ppu->OAMADDR + 2) % 4 == 0)
					v &= 0xE3;

				ppu->oam[ppu->OAMADDR++] = v;

			} else {
				ppu->OAMADDR += 4;
			}
			break;

		case 0x2005:
			if (!ppu->w) {
				ppu->x = v & 0x07;
				SET_CX(ppu->t, v >> 3);

			} else {
				SET_FY(ppu->t, v);
				SET_CY(ppu->t, v >> 3);
			}

			ppu->w = !ppu->w;
			break;

		case 0x2006:
			if (!ppu->w) {
				SET_H(ppu->t, v);

			} else {
				SET_L(ppu->t, v);
				ppu_set_v(ppu, cart, ppu->t, false);
			}

			ppu->w = !ppu->w;
			break;

		case 0x2007:
			ppu_write_vram(ppu, cart, ppu->v & 0x3FFF, v);
			ppu_set_v(ppu, cart, ppu->v + ppu->CTRL.incr, true);
			break;
	}
}


/*** SCROLLING ***/

// https://wiki.nesdev.com/w/index.php/PPU_scrolling

static void ppu_scroll_h(struct ppu *ppu)
{
	uint16_t cx = GET_CX(ppu->v);

	if (cx == 31) {
		SET_CX(ppu->v, 0);
		FLIP_NT_H(ppu->v);

	} else {
		SET_CX(ppu->v, cx + 1);
	}
}

static void ppu_scroll_v(struct ppu *ppu)
{
	uint16_t fy = GET_FY(ppu->v);

	if (fy < 7) {
		SET_FY(ppu->v, fy + 1);

	} else {
		SET_FY(ppu->v, 0);

		uint16_t cy = GET_CY(ppu->v);

		if (cy == 29) {
			SET_CY(ppu->v, 0);
			FLIP_NT_V(ppu->v);

		} else if (cy == 31) {
			SET_CY(ppu->v, 0);

		} else {
			SET_CY(ppu->v, cy + 1);
		}
	}
}

static void ppu_scroll_copy_x(struct ppu *ppu)
{
	SET_CX(ppu->v, GET_CX(ppu->t));
	SET_NT_H(ppu->v, GET_NT(ppu->t));
}

static void ppu_scroll_copy_y(struct ppu *ppu)
{
	SET_CY(ppu->v, GET_CY(ppu->t));
	SET_FY(ppu->v, GET_FY(ppu->t));
	SET_NT_V(ppu->v, GET_NT(ppu->t));
}


/*** BACKGROUND ***/

static uint8_t ppu_read_nt_byte(struct ppu *ppu, struct cart *cart, enum mem type)
{
	return ppu_read_vram(ppu, cart, 0x2000 | (ppu->v & 0x0FFF), type, true);
}

static uint8_t ppu_read_attr_byte(struct ppu *ppu, struct cart *cart, enum mem type)
{
	uint16_t addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x0038) | ((ppu->v >> 2) & 0x0007);

	uint8_t attr = ppu_read_vram(ppu, cart, addr, type, false);
	if (GET_CY(ppu->v) & 0x02) attr >>= 4;
	if (GET_CX(ppu->v) & 0x02) attr >>= 2;

	return attr;
}

static uint8_t ppu_read_tile_byte(struct ppu *ppu, struct cart *cart, uint8_t nt, uint8_t offset)
{
	uint16_t addr = ppu->CTRL.bg_table + (nt * 16) + GET_FY(ppu->v);

	return ppu_read_vram(ppu, cart, addr + offset, ROM_BG, false);
}

static uint8_t ppu_color(uint8_t low_tile, uint8_t high_tile, uint8_t attr, uint8_t shift)
{
	uint8_t color = ((high_tile >> shift) << 1) & 0x02;
	color |= (low_tile >> shift) & 0x01;

	if (color > 0)
		color |= (attr << 2) & 0x0C;

	return color;
}

static void ppu_store_bg(struct ppu *ppu, uint16_t bg_dot)
{
	for (uint8_t x = 0; x < 8; x++)
		ppu->bg[bg_dot + (7 - x)] = ppu_color(ppu->bgl, ppu->bgh, ppu->attr, x);
}

static void ppu_fetch_bg(struct ppu *ppu, struct cart *cart, uint16_t bg_dot)
{
	switch (ppu->dot % 8) {
		case 1:
			ppu->nt = ppu_read_nt_byte(ppu, cart, ROM_BG);
			break;
		case 3:
			ppu->attr = ppu_read_attr_byte(ppu, cart, ROM_BG);
			break;
		case 5:
			ppu->bgl = ppu_read_tile_byte(ppu, cart, ppu->nt, 0);
			break;
		case 7:
			ppu->bgh = ppu_read_tile_byte(ppu, cart, ppu->nt, 8);
			break;
		case 0:
			ppu_store_bg(ppu, bg_dot);
			ppu_scroll_h(ppu);
			break;
	}
}


/*** SPRITES ***/

// https://wiki.nesdev.com/w/index.php/PPU_OAM
// https://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation

#define SPRITE_Y(oam, n)               ((oam)[(n) + 0]) //y position of top of sprite
#define SPRITE_INDEX(oam, n)           ((oam)[(n) + 1]) //8x8 this is the tile number, 8x16 uses bit 0 as pattern table
#define SPRITE_ATTR(oam, n)            ((oam)[(n) + 2]) //sprite attributes defined below
#define SPRITE_X(oam, n)               ((oam)[(n) + 3]) //x position of the left side of the sprite

#define SPRITE_ATTR_PALETTE(attr)      ((attr) & 0x03) //4 to 7
#define SPRITE_ATTR_PRIORITY(attr)     ((attr) & 0x20) //0 in front of bg, 1 behind bg
#define SPRITE_ATTR_FLIP_H(attr)       ((attr) & 0x40) //flip the sprite horizontally
#define SPRITE_ATTR_FLIP_V(attr)       ((attr) & 0x80) //flip the sprite vertically

#define SPRITE_INDEX_8X16_TABLE(index) ((index) & 0x01) //8x16 sprites use the bit 0 as the table
#define SPRITE_INDEX_8X16_TILE(index)  ((index) & 0xFE) //8x16 sprites ignore bit 0 for the tile number

static uint16_t ppu_sprite_addr(struct ppu *ppu, uint16_t row, uint8_t index, uint8_t attr)
{
	uint16_t table = 0;
	uint8_t tile = 0;

	if (SPRITE_ATTR_FLIP_V(attr))
		row = (ppu->CTRL.sprite_h - 1) - row;

	if (ppu->CTRL.sprite_h == 8) {
		table = ppu->CTRL.sprite_table;
		tile = index;

	} else {
		table = SPRITE_INDEX_8X16_TABLE(index) ? 0x1000 : 0x0000;
		tile = SPRITE_INDEX_8X16_TILE(index);

		if (row > 7) {
			tile++;
			row -= 8;
		}
	}

	return table + tile * 16 + row;
}

static void ppu_store_sprite_colors(struct ppu *ppu, uint8_t attr, uint8_t sprite_x, uint8_t id,
	uint8_t low_tile, uint8_t high_tile)
{
	for (uint8_t x = 0; x < 8; x++) {
		uint8_t shift = SPRITE_ATTR_FLIP_H(attr) ? x : 7 - x;
		uint8_t color = ppu_color(low_tile, high_tile, SPRITE_ATTR_PALETTE(attr), shift);
		uint16_t offset = sprite_x + x;

		if (offset < 256 && color != 0) {
			if (!ppu->spr[offset].sprite0)
				ppu->spr[offset].sprite0 = id == 0 && offset != 255;

			if (ppu->spr[offset].color == 0) {
				ppu->spr[offset].color = color + 16;
				ppu->spr[offset].priority = SPRITE_ATTR_PRIORITY(attr);
			}
		}
	}
}

static void ppu_eval_sprites(struct ppu *ppu)
{
	switch (ppu->eval_step) {
		case 1:
			if (ppu->oam_n < 64) {
				uint8_t y = SPRITE_Y(ppu->oam, ppu->OAMADDR);
				int32_t row = ppu->scanline - y;

				if (ppu->soam_n < 8)
					ppu->soam[ppu->soam_n][0] = y;

				if (row >= 0 && row < ppu->CTRL.sprite_h) {
					if (ppu->soam_n == 8) {
						SET_FLAG(ppu->STATUS, FLAG_STATUS_O);
						ppu->overflow = true;

					} else {
						ppu->soam[ppu->soam_n][1] = SPRITE_INDEX(ppu->oam, ppu->OAMADDR);
						ppu->soam[ppu->soam_n][2] = SPRITE_ATTR(ppu->oam, ppu->OAMADDR);
						ppu->soam[ppu->soam_n][3] = SPRITE_X(ppu->oam, ppu->OAMADDR);

						ppu->sprites[ppu->soam_n].id = ppu->oam_n;
					}

					ppu->eval_step++;
					ppu->OAMADDR++;
					return;

				} else if (ppu->soam_n == 8 && !ppu->overflow) {
					ppu->OAMADDR = (ppu->OAMADDR & 0xFC) + ((ppu->OAMADDR + 1) & 0x03);
				}
			}

			ppu->eval_step = 0;
			ppu->oam_n++;
			ppu->OAMADDR += 4;
			break;
		case 0:
		case 2:
		case 4:
		case 6:
			ppu->eval_step++;
			break;
		case 3:
		case 5:
			ppu->eval_step++;
			ppu->OAMADDR++;
			break;
		case 7:
			if (ppu->soam_n < 8)
				ppu->soam_n++;

			ppu->eval_step = 0;
			ppu->oam_n++;
			ppu->OAMADDR++;
			ppu->OAMADDR &= 0xFC;
			break;
	}
}

static void ppu_fetch_sprite(struct ppu *ppu, struct cart *cart)
{
	int32_t n = (ppu->dot - 257) / 8;
	struct sprite *s = &ppu->sprites[n];

	switch (ppu->dot % 8) {
		case 1:
			ppu_read_nt_byte(ppu, cart, ROM_SPRITE);
			break;
		case 3:
			ppu_read_attr_byte(ppu, cart, ROM_SPRITE);
			break;
		case 5: {
			int32_t row = ppu->scanline - ppu->soam[n][0];
			s->addr = ppu_sprite_addr(ppu, (uint16_t) (row > 0 ? row : 0), ppu->soam[n][1], ppu->soam[n][2]);
			s->low_tile = ppu_read_vram(ppu, cart, s->addr, ROM_SPRITE, false);
			break;
		} case 7: {
			uint8_t high_tile = ppu_read_vram(ppu, cart, s->addr + 8, ROM_SPRITE, false);

			if (n < ppu->soam_n)
				ppu_store_sprite_colors(ppu, ppu->soam[n][2], ppu->soam[n][3], s->id, s->low_tile, high_tile);
			break;
		}
	}
}

static void ppu_oam_glitch(struct ppu *ppu)
{
	// https://wiki.nesdev.com/w/index.php/PPU_registers#OAMADDR

	if (ppu->OAMADDR >= 8)
		memcpy(ppu->oam, ppu->oam + ppu->OAMADDR, 8);
}


/*** RENDERING ***/

// https://wiki.nesdev.com/w/index.php/PPU_rendering#Preface

static void ppu_render(struct ppu *ppu, uint16_t dot, bool rendering)
{
	uint16_t addr = 0x3F00;

	if (rendering) {
		bool show_bg = !(dot < 8 && !ppu->MASK.clip_bg) && ppu->MASK.show_bg;
		bool show_sprites = !(dot < 8 && !ppu->MASK.clip_sprites) && ppu->MASK.show_sprites;

		uint8_t color = show_bg ? ppu->bg[dot + ppu->x] : 0;

		if (show_sprites) {
			if (ppu->spr[dot].sprite0 && color != 0)
				SET_FLAG(ppu->STATUS, FLAG_STATUS_S);

			uint8_t sprite_color = ppu->spr[dot].color;
			if (sprite_color != 0 && (color == 0 || !ppu->spr[dot].priority))
				color = sprite_color;
		}

		addr += color;

	} else if (ppu->v >= 0x3F00 && ppu->v < 0x4000) {
		addr = ppu->v;
	}

	uint8_t color = ppu_read_palette(ppu, addr);
	ppu->pixels[ppu->scanline * 256 + dot] = ppu->palette[color];
}


/*** RUN ***/

// https://wiki.nesdev.com/w/index.php/PPU_rendering#Line-by-line_timing

static void ppu_clock(struct ppu *ppu)
{
	if (++ppu->dot > 340) {
		ppu->dot = 0;

		if (++ppu->scanline > 261) {
			ppu->scanline = 0;
			ppu->supress_nmi = false;
			ppu->f = !ppu->f;

			//decay the open bus after 58 frames (~1s)
			if (ppu->decay_high2++ == 58)
				ppu->open_bus &= 0x3F;

			if (ppu->decay_low5++ == 58)
				ppu->open_bus &= 0xC0;
		}
	}
}

static void ppu_memory_access(struct ppu *ppu, struct cart *cart, bool pre_render)
{
	if (ppu->dot >= 1 && ppu->dot <= 256) {
		if (ppu->dot == 1)
			ppu_oam_glitch(ppu);

		ppu_fetch_bg(ppu, cart, ppu->dot + 8);

		if (ppu->dot >= 65 && !pre_render)
			ppu_eval_sprites(ppu);

		if (ppu->dot == 256)
			ppu_scroll_v(ppu);

	} else if (ppu->dot >= 257 && ppu->dot <= 320) {
		ppu_fetch_sprite(ppu, cart);

		ppu->OAMADDR = 0;

		if (ppu->dot == 257) {
			memset(ppu->spr, 0, sizeof(struct spr) * 256);
			ppu_scroll_copy_x(ppu);
		}

	} else if (ppu->dot >= 321 && ppu->dot <= 336) {
		ppu_fetch_bg(ppu, cart, ppu->dot - 328);
	}
}

uint8_t ppu_step(struct ppu *ppu, struct cpu *cpu, struct cart *cart, FRAME_CALLBACK new_frame, void *opaque)
{
	uint8_t got_frame = 0;

	if (ppu->dot == 0) {
		ppu->oam_n = ppu->soam_n = ppu->eval_step = 0;
		ppu->overflow = false;
		memset(ppu->soam, 0xFF, 32);
	}

	if (ppu->dot == 4)
		cart_ppu_scanline_hook(cart, cpu, ppu->scanline);

	if (ppu->scanline <= 239) {
		if (ppu->dot >= 1 && ppu->dot <= 256) //XXX DEFEAT DEVICE: sprite evaluation should begin at cycle 2
			ppu_render(ppu, ppu->dot - 1, ppu->MASK.rendering);

		if (ppu->MASK.rendering)
			ppu_memory_access(ppu, cart, false);

	} else if (ppu->scanline == 240) {
		if (ppu->dot == 0) {
			ppu_set_bus_v(ppu, cart, ppu->v);

			if (!ppu->palette_write)
				memset(ppu->pixels, 0, 256 * 240 * 4);

			new_frame(ppu->pixels, opaque);
			got_frame = 1;
		}

	} else if (ppu->scanline == 241) {
		if (ppu->dot == 1 && !ppu->supress_nmi) {
			SET_FLAG(ppu->STATUS, FLAG_STATUS_V);

			if (ppu->CTRL.nmi_enabled)
				cpu_nmi(cpu, true);
		}

	} else if (ppu->scanline == 261) {
		if (ppu->dot == 0) { //XXX DEFEAT DEVICE: these should be cleared at 1?
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_O);
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_S);
		}

		if (ppu->dot == 1)
			UNSET_FLAG(ppu->STATUS, FLAG_STATUS_V);

		if (ppu->MASK.rendering) {
			if (ppu->dot >= 280 && ppu->dot <= 304)
				ppu_scroll_copy_y(ppu);

			ppu_memory_access(ppu, cart, true);

			if (ppu->dot == 339 && ppu->f)
				ppu->dot++;
		}
	}

	ppu_clock(ppu);

	return got_frame;
}


/*** INIT & DESTROY ***/

void ppu_init(struct ppu **ppu_out)
{
	*ppu_out = calloc(1, sizeof(struct ppu));
}

void ppu_destroy(struct ppu **ppu_out)
{
	if (!ppu_out || *ppu_out) return;

	free(*ppu_out);
	*ppu_out = NULL;
}

static void ppu_generate_emphasis_tables(struct ppu *ppu)
{
	memcpy(ppu->palettes[0], PALETTE, sizeof(uint32_t) * 64);

	for (uint8_t x = 1; x < 8; x++) {
		for (uint8_t y = 0; y < 64; y++) {
			uint32_t rgba = PALETTE[y];

			uint32_t r = (uint32_t) ((float) (rgba & 0x000000FF) * EMPHASIS[x][0]);
			uint32_t g = (uint32_t) ((float) ((rgba & 0x0000FF00) >> 8) * EMPHASIS[x][1]);
			uint32_t b = (uint32_t) ((float) ((rgba & 0x00FF0000) >> 16) * EMPHASIS[x][2]);

			ppu->palettes[x][y] = r | (g << 8) | (b << 16) | 0xFF000000;
		}
	}
}

void ppu_reset(struct ppu *ppu)
{
	memset(ppu, 0, sizeof(struct ppu));

	memcpy(ppu->palette_ram, POWER_UP_PALETTE, 32);

	ppu_generate_emphasis_tables(ppu);
	ppu->palette = ppu->palettes[0];

	ppu->CTRL.incr = 1;
	ppu->CTRL.sprite_h = 8;
	ppu->MASK.grayscale = 0x3F;
}
