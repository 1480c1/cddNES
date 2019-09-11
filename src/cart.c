#include "cart.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*** MAPPING ***/

#define PRG_SLOT 0x1000
#define CHR_SLOT 0x0400

#define PRG_SHIFT 12
#define CHR_SHIFT 10

#define ROM ROM_SPRITE

struct memory {
	uint8_t *data;
	size_t size;
};

struct map {
	enum mem type;
	uint8_t *ptr;
};

struct asset {
	struct map map[2][16];
	uint16_t mask;
	uint8_t shift;
	struct memory rom;
	struct memory ram;
	struct memory ciram;
	size_t sram;
	size_t wram;
};

static uint8_t map_read(struct asset *asset, uint8_t index, uint16_t addr, bool *hit)
{
	uint8_t *mapped_addr = asset->map[index][addr >> asset->shift].ptr;

	if (mapped_addr) {
		if (hit) *hit = true;
		return mapped_addr[addr & asset->mask];
	}

	if (hit) *hit = false;
	return 0;
}

static void map_write(struct asset *asset, uint8_t index, uint16_t addr, uint8_t v)
{
	struct map *m = &asset->map[index][addr >> asset->shift];

	if (m->ptr && (m->type & RAM))
		m->ptr[addr & asset->mask] = v;
}

static void map_unmap(struct asset *asset, uint8_t index, uint16_t addr)
{
	asset->map[index][addr >> asset->shift].ptr = NULL;
}

static void cart_map(struct asset *asset, enum mem type, uint16_t addr, uint16_t bank, uint8_t bank_size_kb)
{
	struct memory *mem = ((type & CIRAM) == CIRAM) ? &asset->ciram : (type & RAM) ? &asset->ram : &asset->rom;

	int32_t start_slot = addr >> asset->shift;
	int32_t bank_size_bytes = bank_size_kb * 0x0400;
	int32_t bank_offset = bank * bank_size_bytes;
	int32_t end_slot = start_slot + (bank_size_bytes >> asset->shift);

	for (int32_t x = start_slot, y = 0; x < end_slot; x++, y++) {
		struct map *m = &asset->map[type & 0x0F][x];

		m->ptr = mem->data + (bank_offset + (y << asset->shift)) % mem->size;
		m->type = type;
	}
}

static void cart_map_ciram_buf(struct asset *asset, uint8_t dest, enum mem type, uint8_t *buf)
{
	asset->map[0][dest + 8].ptr = buf;
	asset->map[0][dest + 8].type = type;

	if (dest < 4) {
		asset->map[0][dest + 12].ptr = buf;
		asset->map[0][dest + 12].type = type;
	}
}

static void cart_map_ciram_slot(struct asset *asset, uint8_t dest, uint8_t src)
{
	cart_map_ciram_buf(asset, dest, CIRAM, asset->ciram.data + src * CHR_SLOT);
}

static void cart_map_ciram(struct asset *asset, enum mirror mirror)
{
	for (uint8_t x = 0; x < 8; x++)
		cart_map_ciram_slot(asset, x, (mirror >> (x * 4)) & 0xF);
}

static uint8_t cart_bus_conflict(struct asset *asset, uint16_t addr, uint8_t v)
{
	bool hit = false;
	uint8_t v0 = map_read(asset, 0, addr, &hit);

	if (hit)
		return v & v0;

	return v;
}


/*** CART & MAPPERS ***/

#define KB(b) ((b) / 0x0400)

struct cart {
	struct nes_header hdr;
	struct asset prg;
	struct asset chr;

	size_t sram_dirty;
	uint64_t read_counter;
	uint64_t cycle;

	bool ram_enable;
	uint8_t prg_mode;
	uint8_t chr_mode;
	uint8_t REG[8];
	uint8_t PRG[8];
	uint8_t CHR[8];

	struct {
		bool enable;
		bool reload;
		bool pending;
		uint16_t counter;
		uint16_t value;
		uint8_t period;
		int16_t scanline;
		bool cycle;
	} irq;

	struct {
		bool use256;
		uint64_t cycle;
		uint8_t n;
	} mmc1;

	struct {
		uint8_t bank_update;
	} mmc3;

	struct {
		uint8_t exram[0x0400];

		uint8_t exram_mode;
		uint8_t fill_tile;
		uint8_t fill_attr;
		uint8_t exram1;
		uint16_t multiplicand;
		uint16_t multiplier;
		uint16_t chr_bank_upper;
		enum mem active_map;
		bool nt_latch;
		bool exram_latch;
		bool large_sprites;
		bool rendering_enabled;
		bool in_frame;

		struct {
			bool enable;
			bool right;
			bool fetch;
			uint16_t htile;
			uint16_t scroll;
			uint8_t scroll_reload;
			uint8_t tile;
			uint8_t bank;
		} vs;
	} mmc5;

	struct {
		bool is2;
		uint16_t type;
	} vrc;
};

#include "mapper/fcg.c"
#include "mapper/fme7.c"
#include "mapper/mapper.c"
#include "mapper/mmc1.c"
#include "mapper/mmc2.c"
#include "mapper/mmc3.c"
#include "mapper/mmc5.c"
#include "mapper/namco.c"
#include "mapper/vrc.c"
#include "mapper/vrc6.c"
#include "mapper/vrc7.c"


/*** READ & WRITE ***/

uint8_t cart_prg_read(struct cart *cart, struct cpu *cpu, uint16_t addr, bool *mem_hit)
{
	switch (cart->hdr.mapper) {
		case 5:  return mmc5_prg_read(cart, cpu, addr, mem_hit);
		case 19: return namco_prg_read(cart, addr, mem_hit);
		default:
			return map_read(&cart->prg, 0, addr, mem_hit);
	}
}

void cart_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	switch (cart->hdr.mapper) {
		case 1:  mmc1_prg_write(cart, addr, v);       break;
		case 4:  mmc3_prg_write(cart, cpu, addr, v);  break;
		case 5:  mmc5_prg_write(cart, addr, v);       break;
		case 9:  mmc2_prg_write(cart, addr, v);       break;
		case 10: mmc2_prg_write(cart, addr, v);       break;
		case 19: namco_prg_write(cart, cpu, addr, v); break;
		case 21: vrc_prg_write(cart, cpu, addr, v);   break;
		case 22: vrc_prg_write(cart, cpu, addr, v);   break;
		case 24: vrc6_prg_write(cart, cpu, addr, v);  break;
		case 26: vrc6_prg_write(cart, cpu, addr, v);  break;
		case 23: vrc_prg_write(cart, cpu, addr, v);   break;
		case 25: vrc_prg_write(cart, cpu, addr, v);   break;
		case 69: fme7_prg_write(cart, cpu, addr, v);  break;
		case 85: vrc7_prg_write(cart, cpu, addr, v);  break;
		case 16:
		case 159: fcg_prg_write(cart, cpu, addr, v);  break;
		case 0:
		case 2:
		case 3:
		case 7:
		case 11:
		case 13:
		case 30:
		case 31:
		case 34:
		case 38:
		case 66:
		case 70:
		case 71:
		case 77:
		case 78:
		case 79:
		case 87:
		case 89:
		case 93:
		case 94:
		case 97:
		case 101:
		case 107:
		case 111:
		case 113:
		case 140:
		case 145:
		case 146:
		case 148:
		case 149:
		case 152:
		case 180:
		case 184:
		case 185: mapper_prg_write(cart, addr, v);   break;
	}
}

uint8_t cart_chr_read(struct cart *cart, uint16_t addr, enum mem type, bool nt)
{
	bool chr = addr < 0x2000;

	if (chr) {
		switch (cart->hdr.mapper) {
			case 5:  return mmc5_chr_read(cart, addr, type);
			case 9:
			case 10: mmc2_chr_read(cart, addr); break;
		}

	} else {
		switch (cart->hdr.mapper) {
			case 5: return mmc5_nt_read_hook(cart, addr, type, nt);
		}
	}

	return map_read(&cart->chr, 0, addr, NULL);
}

void cart_chr_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	map_write(&cart->chr, 0, addr, v);
}


/*** HOOKS ***/

void cart_ppu_a12_toggle(struct cart *cart)
{
	switch (cart->hdr.mapper) {
		case 4: mmc3_ppu_a12_toggle(cart);
	}
}

void cart_ppu_write_hook(struct cart *cart, uint16_t addr, uint8_t v)
{
	switch (cart->hdr.mapper) {
		case 5: mmc5_ppu_write_hook(cart, addr, v); break;
	}
}

void cart_ppu_scanline_hook(struct cart *cart, struct cpu *cpu, uint16_t scanline)
{
	switch (cart->hdr.mapper) {
		case 5: mmc5_scanline(cart, cpu, scanline); break;
	}
}

bool cart_block_2007(struct cart *cart)
{
	switch (cart->hdr.mapper) {
		case 185: return mapper_block_2007(cart);
	}

	return false;
}


/*** SRAM ***/

size_t cart_sram_dirty(struct cart *cart)
{
	if (!cart->hdr.battery) return 0;

	size_t dirty_size = cart->sram_dirty;
	cart->sram_dirty = 0;

	return dirty_size;
}

void cart_sram_get(struct cart *cart, uint8_t *buf, size_t size)
{
	memcpy(buf, cart->prg.ram.data, size);
	cart->sram_dirty = 0;
}


/*** RUN ***/

void cart_step(struct cart *cart, struct cpu *cpu)
{
	switch (cart->hdr.mapper) {
		case 4:  mmc3_step(cart, cpu);  break;
		case 19: namco_step(cart, cpu); break;
		case 21: vrc_step(cart, cpu);   break;
		case 23: vrc_step(cart, cpu);   break;
		case 24: vrc_step(cart, cpu);   break;
		case 26: vrc_step(cart, cpu);   break;
		case 25: vrc_step(cart, cpu);   break;
		case 69: fme7_step(cart, cpu);  break;
		case 85: vrc_step(cart, cpu);   break;
		case 16:
		case 159: fcg_step(cart, cpu);  break;
	}

	cart->cycle++;
}


/*** INIT & DESTROY ***/

static void cart_parse_header(uint8_t *rom, struct nes_header *hdr)
{
	if (rom[0] == 'U' && rom[1] == 'N' && rom[2] == 'I' && rom[3] == 'F')
		assert(!"UNIF format unsupported");

	if (!(rom[0] == 'N' && rom[1] == 'E' && rom[2] == 'S' && rom[3] == 0x1A))
		assert(!"iNES magic header value 'N' 'E' 'S' 0x1A not found");

	// archaic iNES
	hdr->offset = 16;
	hdr->prg = rom[4];
	hdr->chr = rom[5];
	hdr->mirroring = (rom[6] & 0x08) ? MIRROR_FOUR : (rom[6] & 0x01) ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;
	hdr->battery = rom[6] & 0x02;
	hdr->trainer = rom[6] & 0x04;
	hdr->mapper = rom[6] >> 4;

	// modern iNES
	if ((rom[7] & 0x0C) == 0 && rom[12] == 0 && rom[13] == 0 && rom[14] == 0 && rom[15] == 0) {
		hdr->mapper |= rom[7] & 0xF0;

	// NES 2.0
	} else if (((rom[7] & 0x0C) >> 2) == 0x02) {
		hdr->has_nes2 = true;
		hdr->mapper |= rom[7] & 0xF0;
		hdr->mapper |= (rom[8] & 0x0F) << 8;
		hdr->nes2.submapper = rom[8] >> 4;

		uint8_t volatile_shift = rom[10] & 0x0F;
		hdr->nes2.prg_wram = volatile_shift ? 64 << volatile_shift : 0;

		uint8_t non_volatile_shift = (rom[10] & 0xF0) >> 4;
		hdr->nes2.prg_sram = non_volatile_shift ? 64 << non_volatile_shift : 0;

		volatile_shift = rom[11] & 0x0F;
		hdr->nes2.chr_wram = volatile_shift ? 64 << volatile_shift : 0;

		non_volatile_shift = (rom[11] & 0xF0) >> 4;
		hdr->nes2.chr_sram = non_volatile_shift ? 64 << non_volatile_shift : 0;
	}
}

static void cart_log_header(struct nes_header *hdr)
{
	nes_log("Mapper: %u", hdr->mapper);
	nes_log("PRG Size: %uKB", KB(hdr->prg * 0x4000));
	nes_log("CHR Size: %uKB", KB(hdr->chr * 0x2000));
	nes_log("Mirroring: %s", hdr->mirroring == MIRROR_VERTICAL ? "Vertical" :
		hdr->mirroring == MIRROR_HORIZONTAL ? "Horizontal" : "Four Screen");
	nes_log("Trainer: %s", hdr->trainer ? "true" : "false");
	nes_log("PRG RAM Battery: %s", hdr->battery ? "true" : "false");

	if (hdr->has_nes2) {
		nes_log("NES 2.0 Submapper: %x", hdr->nes2.submapper);
		nes_log("NES 2.0 PRG RAM (volatile): %uKB", KB(hdr->nes2.prg_wram));
		nes_log("NES 2.0 PRG RAM (non-volatile): %uKB", KB(hdr->nes2.prg_sram));
		nes_log("NES 2.0 CHR RAM (volatile): %uKB", KB(hdr->nes2.chr_wram));
		nes_log("NES 2.0 CHR RAM (non-volatile): %uKB", KB(hdr->nes2.chr_sram));
	}
}

void cart_init(struct cart **cart_out, uint8_t *rom, size_t rom_len,
	uint8_t *sram, size_t sram_len, struct nes_header *hdr)
{
	struct cart *cart = *cart_out = calloc(1, sizeof(struct cart));
	cart->prg.mask = PRG_SLOT - 1;
	cart->chr.mask = CHR_SLOT - 1;
	cart->prg.shift = PRG_SHIFT;
	cart->chr.shift = CHR_SHIFT;

	nes_log("ROM size: %uKB", rom_len / 0x0400);

	if (sram_len > 0x2000)
		assert(!"SRAM is lager than 8K");

	if (hdr) {
		cart->hdr = *hdr;

	} else {
		if (rom_len < 16)
			assert(!"ROM is less than 16 bytes");

		cart_parse_header(rom, &cart->hdr);
	}

	cart_log_header(&cart->hdr);

	cart->prg.rom.size = cart->hdr.prg * 0x4000;
	cart->chr.rom.size = cart->hdr.chr * 0x2000;

	cart->prg.sram = 0x2000;
	cart->prg.wram = 0x01E000;
	cart->chr.ciram.size = 0x4000;

	if (cart->hdr.has_nes2) {
		cart->prg.wram = cart->hdr.nes2.prg_wram;
		cart->prg.sram = cart->hdr.nes2.prg_sram;
		cart->chr.wram = cart->hdr.nes2.chr_wram;
		cart->chr.sram = cart->hdr.nes2.chr_sram;
	}

	if (cart->chr.wram == 0)
		cart->chr.wram = 0x8000;

	cart->prg.ram.size = cart->prg.wram + cart->prg.sram;
	cart->chr.ram.size = cart->chr.wram + cart->chr.sram;

	uint16_t trainer = cart->hdr.trainer ? 512 : 0;
	if (cart->hdr.offset + trainer + cart->prg.rom.size > rom_len)
		assert(!"ROM is not large enough to support PRG ROM size in iNES header");

	cart->prg.ram.data = calloc(cart->prg.ram.size, 1);
	if (sram && sram_len > 0)
		memcpy(cart->prg.ram.data, sram, sram_len);

	cart->prg.rom.data = calloc(cart->prg.rom.size, 1);
	memcpy(cart->prg.rom.data, rom + cart->hdr.offset + trainer, cart->prg.rom.size);
	cart_map(&cart->prg, ROM, 0x8000, 0, 32);

	cart->chr.ram.data = calloc(cart->chr.ram.size, 1);
	cart->chr.ciram.data = calloc(cart->chr.ciram.size, 1);
	cart_map_ciram(&cart->chr, cart->hdr.mirroring);

	if (cart->chr.rom.size > 0) {
		int32_t chr_len = (int32_t) rom_len - (int32_t) cart->hdr.offset - trainer - (int32_t) cart->prg.rom.size;

		if (chr_len <= 0)
			assert(!"ROM is not large enough to support CHR ROM size in iNES header");

		if (chr_len > (int32_t) cart->chr.rom.size)
			chr_len = (int32_t) cart->chr.rom.size;

		cart->chr.rom.data = calloc(chr_len, 1);
		memcpy(cart->chr.rom.data, rom + cart->hdr.offset + trainer + cart->prg.rom.size, chr_len);
	}
	cart_map(&cart->chr, cart->chr.rom.size > 0 ? ROM : RAM, 0x0000, 0, 8);

	switch (cart->hdr.mapper) {
		case 1:   mmc1_init(cart);    break;
		case 4:   mmc3_init(cart);    break;
		case 5:   mmc5_init(cart);    break;
		case 9:   mmc2_init(cart);    break;
		case 10:  mmc2_init(cart);    break;
		case 19:  namco_init(cart);   break;
		case 21:  vrc2_4_init(cart);  break;
		case 22:  vrc2_4_init(cart);  break;
		case 24:  vrc_init(cart);     break;
		case 26:  vrc_init(cart);     break;
		case 23:  vrc2_4_init(cart);  break;
		case 25:  vrc2_4_init(cart);  break;
		case 69:  fme7_init(cart);    break;
		case 85:  vrc_init(cart);     break;
		case 16:
		case 159: fcg_init(cart);     break;
		case 0:
		case 2:
		case 3:
		case 7:
		case 11:
		case 13:
		case 30:
		case 31:
		case 34:
		case 38:
		case 66:
		case 70:
		case 71:
		case 77:
		case 78:
		case 79:
		case 87:
		case 89:
		case 93:
		case 94:
		case 97:
		case 101:
		case 107:
		case 111:
		case 113:
		case 140:
		case 145:
		case 146:
		case 148:
		case 149:
		case 152:
		case 180:
		case 184:
		case 185: mapper_init(cart);  break;

		default:
			assert(!"Unsupported mapper");
			break;
	}
}

void cart_destroy(struct cart **cart_out)
{
	if (!cart_out || !*cart_out) return;

	struct cart *cart = *cart_out;

	free(cart->prg.rom.data);
	free(cart->prg.ram.data);
	free(cart->chr.rom.data);
	free(cart->chr.ram.data);
	free(cart->chr.ciram.data);

	free(*cart_out);
	*cart_out = NULL;
}
