static void vrc6_map_ppu(struct cart *cart)
{
	bool reg_mode = cart->REG[0] & 0x20;
	uint8_t chr_mode = cart->REG[0] & 0x03;

	for (uint8_t x = 0; x < 8; x++) {
		bool odd_bank = x & 1;
		bool ignore_lsb = chr_mode == 1 || (chr_mode > 1 && x > 3);
		uint8_t mask = reg_mode && ignore_lsb ? 0xFE : 0xFF;
		uint8_t or = reg_mode && odd_bank && ignore_lsb ? 1 : 0;
		uint8_t n = chr_mode == 1 ? x / 2 : chr_mode > 1 && x > 3 ? (x + 4) / 2 : x;

		cart_map(&cart->chr, ROM, x * 0x0400, (cart->CHR[n] & mask) | or, 1);
	}

	uint8_t c[4] = {0, 0, 0, 0};

	switch (cart->REG[0] & 0x2F) {
		case 0x20:
		case 0x27:
			c[0] = cart->CHR[6] & 0xFE;
			c[1] = c[0] + 1;
			c[2] = cart->CHR[7] & 0xFE;
			c[3] = c[2] + 1;
			break;
		case 0x23:
		case 0x24:
			c[0] = cart->CHR[6] & 0xFE;
			c[1] = cart->CHR[7] & 0xFE;
			c[2] = c[0] + 1;
			c[3] = c[1] + 1;
			break;
		case 0x28:
		case 0x2F:
			c[0] = c[1] = cart->CHR[6] & 0xFE;
			c[2] = c[3] = cart->CHR[7] & 0xFE;
			break;
		case 0x2B:
		case 0x2C:
			c[0] = c[2] = cart->CHR[6] | 1;
			c[1] = c[3] = cart->CHR[7] | 1;
			break;
		default:
			switch (cart->REG[0] & 0x07) {
				case 0:
				case 6:
				case 7:
					c[0] = c[1] = cart->CHR[6];
					c[2] = c[3] = cart->CHR[7];
					break;
				case 1:
				case 5:
					c[0] = cart->CHR[4];
					c[1] = cart->CHR[5];
					c[2] = cart->CHR[6];
					c[3] = cart->CHR[7];
					break;
				case 2:
				case 3:
				case 4:
					c[0] = c[2] = cart->CHR[6];
					c[1] = c[3] = cart->CHR[7];
					break;
			}
	}

	for (uint8_t x = 0; x < 4; x++) {
		if (cart->REG[0] & 0x10) {
			cart_map_ciram_buf(&cart->chr, x, ROM, cart->chr.rom.data + 0x400 * c[x]);
		} else {
			cart_map_ciram_slot(&cart->chr, x, ~c[x] & 0x01);
		}
	}
}

static void vrc6_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x6000 && addr < 0x8000) {
		map_write(&cart->prg, 0, addr, v);

	} else if (addr >= 0x8000) {
		if (cart->hdr.mapper == 26)
			addr = (addr & 0xFFFC) | ((addr & 0x01) << 1) | ((addr & 0x02) >> 1);

		switch (addr & 0xF003) {
			case 0x8000: //PRG first bank
			case 0x8001:
			case 0x8002:
			case 0x8003:
				cart_map(&cart->prg, ROM, 0x8000, v & 0x0F, 16);
				break;
			case 0x9000: //Pulse0 expansion audio
			case 0x9001:
			case 0x9002:
			case 0x9003:
				break;
			case 0xA000: //Pulse1 expansion audio
			case 0xA001:
			case 0xA002:
			case 0xA003:
				break;
			case 0xC000: //PRG second bank
			case 0xC001:
			case 0xC002:
			case 0xC003:
				cart_map(&cart->prg, ROM, 0xC000, v & 0x1F, 8);
				break;
			case 0xB000: //Sawtooth expansion audio
			case 0xB001:
			case 0xB002:
				break;
			case 0xB003: //Mirroring, PPU banking
				cart->REG[0] = v;
				vrc6_map_ppu(cart);
				break;
			case 0xD000: //CHR banking
			case 0xD001:
			case 0xD002:
			case 0xD003:
				cart->CHR[addr - 0xD000] = v;
				vrc6_map_ppu(cart);
				break;
			case 0xE000:
			case 0xE001:
			case 0xE002:
			case 0xE003:
				cart->CHR[4 + (addr - 0xE000)] = v;
				vrc6_map_ppu(cart);
				break;
			case 0xF000: //IRQ control
				cart->irq.value = v;
				break;
			case 0xF001:
				vrc_set_irq_control(cart, cpu, v);
				break;
			case 0xF002:
				vrc_ack_irq(cart, cpu);
				break;
			default:
				nes_log("Uncaught VRC6 write %x: %x", addr, v);
		}
	}
}
