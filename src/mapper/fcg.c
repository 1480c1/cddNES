// https://wiki.nesdev.com/w/index.php/INES_Mapper_016
// https://wiki.nesdev.com/w/index.php/INES_Mapper_159

static void fcg_init(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x4000) - 1;

	cart_map(&cart->prg, ROM, 0xC000, last_bank, 16);

	cart_map_ciram(&cart->chr, MIRROR_VERTICAL);
}

static void fcg_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	uint16_t addr_low = 0x6000;
	uint16_t addr_high = 0x7FFF;
	uint16_t addr_sub = 0;
	bool alt = (cart->hdr.mapper == 16 && cart->hdr.nes2.submapper == 5) || cart->hdr.mapper == 159;

	if (alt) {
		addr_low = 0x8000;
		addr_high = 0xFFFF;
		addr_sub = 0x2000;
	}

	if (addr >= addr_low && addr <= addr_high) {
		addr -= addr_sub;
		addr &= 0xE00F;

		switch (addr) {
			case 0x6000: //CHR
			case 0x6001:
			case 0x6002:
			case 0x6003:
			case 0x6004:
			case 0x6005:
			case 0x6006:
			case 0x6007:
				cart_map(&cart->chr, ROM, (addr - 0x6000) * 0x0400, v, 1);
				break;
			case 0x6008: //PRG
				cart_map(&cart->prg, ROM, 0x8000, v & 0x0F, 16);
				break;
			case 0x6009: //Mirroring
				switch (v & 0x03) {
					case 0: cart_map_ciram(&cart->chr, MIRROR_VERTICAL);   break;
					case 1: cart_map_ciram(&cart->chr, MIRROR_HORIZONTAL); break;
					case 2: cart_map_ciram(&cart->chr, MIRROR_SINGLE0);    break;
					case 3: cart_map_ciram(&cart->chr, MIRROR_SINGLE1);    break;
				}
				break;
			case 0x600A: //IRQ
				cart->irq.enable = v & 0x01;
				cpu_irq(cpu, IRQ_MAPPER, false);

				if (alt)
					cart->irq.counter = cart->irq.value;
				break;
			case 0x600B:
				if (alt) {
					cart->irq.value = (cart->irq.value & 0xFF00) | v;
				} else {
					cart->irq.counter = (cart->irq.counter & 0xFF00) | v;
				}
				break;
			case 0x600C:
				if (alt) {
					cart->irq.value = (cart->irq.value & 0x00FF) | ((uint16_t) v << 8);
				} else {
					cart->irq.counter = (cart->irq.counter & 0x00FF) | ((uint16_t) v << 8);
				}
				break;
			case 0x600D: //EEPROM write
				break;
			default:
				nes_log("Uncaught Bandai FCG write %x: %x", addr, v);
		}
	}
}

static void fcg_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->irq.enable) {
		if (cart->irq.counter == 0xFFFE) {
			cpu_irq(cpu, IRQ_MAPPER, true);
			cart->irq.enable = false;
		} else {
			cart->irq.counter--;
		}
	}
}
