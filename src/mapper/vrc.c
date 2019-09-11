// https://wiki.nesdev.com/w/index.php/VRC2_and_VRC4
// https://wiki.nesdev.com/w/index.php/VRC_IRQ

enum vrc {
	VRC2A = 0x1600,
	VRC2B = 0x1703,
	VRC2C = 0x1903,
	VRC4A = 0x1501,
	VRC4B = 0x1901,
	VRC4C = 0x1502,
	VRC4D = 0x1902,
	VRC4E = 0x1702,
	VRC4F = 0x1701,
};

#define FIX_ADDR(addr, n)   (((addr) & 0xF000) | (n))
#define MATCH_ADDR(addr, x) (((addr) & (x)) == (x))
#define TEST_ADDR(addr, x)  ((addr) & (x))

static void vrc_prg_map(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

	if (!cart->prg_mode) {
		cart_map(&cart->prg, ROM, 0x8000, cart->PRG[0], 8);
		cart_map(&cart->prg, ROM, 0xC000, last_bank - 1, 8);

	} else {
		cart_map(&cart->prg, ROM, 0x8000, last_bank - 1, 8);
		cart_map(&cart->prg, ROM, 0xC000, cart->PRG[0], 8);
	}
}

static void vrc_init(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

	cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);

	cart->irq.scanline = 341;
}

static void vrc2_4_init(struct cart *cart)
{
	vrc_init(cart);

	cart->vrc.type = (cart->hdr.mapper << 8) | cart->hdr.nes2.submapper;
	cart->vrc.is2 = (cart->vrc.type == VRC2A || cart->vrc.type == VRC2B || cart->vrc.type == VRC2C);

	vrc_prg_map(cart);
}

static uint16_t vrc_repin(uint16_t addr, uint8_t p0, uint8_t p1, uint8_t p2)
{
	if (MATCH_ADDR(addr, p0)) return FIX_ADDR(addr, 3);
	if (MATCH_ADDR(addr, p1)) return FIX_ADDR(addr, 2);
	if (MATCH_ADDR(addr, p2)) return FIX_ADDR(addr, 1);

	return addr & 0xF000;
}

static uint16_t vrc_legacy_repin(uint16_t addr, uint8_t p0a, uint8_t p0b, uint8_t p1, uint8_t p2)
{
	if (MATCH_ADDR(addr, p0a) || MATCH_ADDR(addr, p0b)) return FIX_ADDR(addr, 3);
	if (TEST_ADDR(addr, p1)) return FIX_ADDR(addr, 2);
	if (TEST_ADDR(addr, p2)) return FIX_ADDR(addr, 1);

	return addr & 0xF000;
}

static uint16_t vrc_rejigger_pins(struct cart *cart, uint16_t addr)
{
	// precise mapper/submapper selection
	switch (cart->vrc.type) {
		case VRC2A:
		case VRC2C:
		case VRC4B: return vrc_repin(addr, 0x03, 0x01, 0x02);
		case VRC2B:
		case VRC4F: return vrc_repin(addr, 0x03, 0x02, 0x01);
		case VRC4A: return vrc_repin(addr, 0x06, 0x04, 0x02);
		case VRC4C: return vrc_repin(addr, 0xC0, 0x80, 0x40);
		case VRC4D: return vrc_repin(addr, 0x0C, 0x04, 0x08);
		case VRC4E: return vrc_repin(addr, 0x0C, 0x08, 0x04);
	}

	// legacy emulation
	switch (cart->hdr.mapper) {
		case 23: return vrc_legacy_repin(addr, 0x0C, 0x03, 0x0A, 0x05);
		case 25: return vrc_legacy_repin(addr, 0x0C, 0x03, 0x05, 0x0A);
		case 21:
		default: return vrc_legacy_repin(addr, 0x06, 0xC0, 0x84, 0x42);
	}
}

static void vrc_set_irq_control(struct cart *cart, struct cpu *cpu, uint8_t v)
{
	cart->irq.reload = v & 0x01;
	cart->irq.enable = v & 0x02;
	cart->irq.cycle = v & 0x04;

	if (cart->irq.enable) {
		cart->irq.counter = cart->irq.value;
		cart->irq.scanline = 341;
	}

	cpu_irq(cpu, IRQ_MAPPER, false);
}

static void vrc_ack_irq(struct cart *cart, struct cpu *cpu)
{
	cpu_irq(cpu, IRQ_MAPPER, false);
	cart->irq.enable = cart->irq.reload;
}

static void vrc_mirror(struct cart *cart, uint8_t v)
{
	switch (v) {
		case 0: cart_map_ciram(&cart->chr, MIRROR_VERTICAL);   break;
		case 1: cart_map_ciram(&cart->chr, MIRROR_HORIZONTAL); break;
		case 2: cart_map_ciram(&cart->chr, MIRROR_SINGLE0);    break;
		case 3: cart_map_ciram(&cart->chr, MIRROR_SINGLE1);    break;
	}
}

static void vrc_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x6000 && addr < 0x8000) {
		map_write(&cart->prg, 0, addr, v);

	} else if (addr >= 0x8000) {
		addr = vrc_rejigger_pins(cart, addr);

		switch (addr) {
			case 0x8000: //PRG first/last banks
			case 0x8001:
			case 0x8002:
			case 0x8003:
				cart->PRG[0] = v & 0x1F;
				vrc_prg_map(cart);
				break;
			case 0x9000: //Mirroring
			case 0x9001:
				vrc_mirror(cart, v & (cart->vrc.is2 ? 0x01 : 0x03));
				break;
			case 0x9002: //PRG mode
			case 0x9003:
				if (cart->vrc.is2) {
					vrc_prg_write(cart, cpu, 0x9000, v);
					return;
				}

				cart->prg_mode = v & 0x02;
				vrc_prg_map(cart);
				break;
			case 0xA000: //PRG middle bank
			case 0xA001:
			case 0xA002:
			case 0xA003:
				cart_map(&cart->prg, ROM, 0xA000, v & 0x1F, 8);
				break;
			case 0xB000: //CHR
			case 0xB001:
			case 0xB002:
			case 0xB003:
			case 0xC000:
			case 0xC001:
			case 0xC002:
			case 0xC003:
			case 0xD000:
			case 0xD001:
			case 0xD002:
			case 0xD003:
			case 0xE000:
			case 0xE001:
			case 0xE002:
			case 0xE003: {
				uint8_t slot = (((addr - 0xB000) & 0xF000) >> 12) * 2 + (addr & 0x0003) / 2;

				if (addr % 2 == 0) {
					cart->CHR[slot] = (cart->CHR[slot] & 0x01F0) | (v & 0x0F);
				} else {
					cart->CHR[slot] = (cart->CHR[slot] & 0x000F) | ((v & 0x1F) << 4);
				}

				uint16_t bank = cart->vrc.type == VRC2A ? cart->CHR[slot] >> 1 : cart->CHR[slot];
				cart_map(&cart->chr, ROM, slot * 0x0400, bank, 1);
				break;
			}
			case 0xF000: //IRQ
				cart->irq.value = (cart->irq.value & 0xF0) | (v & 0x0F);
				break;
			case 0xF001:
				cart->irq.value = (cart->irq.value & 0x0F) | ((v & 0x0F) << 4);
				break;
			case 0xF002:
				vrc_set_irq_control(cart, cpu, v);
				break;
			case 0xF003:
				vrc_ack_irq(cart, cpu);
				break;
			default:
				nes_log("Uncaught VRC2/4 write %x: %x", addr, v);
		}
	}
}

static void vrc_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->vrc.is2)
		return;

	bool clock = false;

	if (cart->irq.cycle)
		clock = true;

	if (cart->irq.scanline <= 0) {
		if (!cart->irq.cycle)
			clock = true;
		cart->irq.scanline += 341;
	}
	cart->irq.scanline -= 3;

	if (cart->irq.enable && clock) {
		if (cart->irq.counter == 0xFF) {
			cpu_irq(cpu, IRQ_MAPPER, true);
			cart->irq.counter = cart->irq.value;

		} else {
			cart->irq.counter++;
		}
	}
}
