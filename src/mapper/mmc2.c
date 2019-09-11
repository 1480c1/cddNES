// https://wiki.nesdev.com/w/index.php/MMC2
// https://wiki.nesdev.com/w/index.php/MMC4

static void mmc2_init(struct cart *cart)
{
	if (cart->hdr.mapper == 9) {
		uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

		cart_map(&cart->prg, ROM, 0xA000, last_bank - 2, 8);
		cart_map(&cart->prg, ROM, 0xC000, last_bank - 1, 8);
		cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	} else {
		uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x4000) - 1;

		cart_map(&cart->prg, ROM, 0x8000, 0, 16);
		cart_map(&cart->prg, ROM, 0xC000, last_bank, 16);

		if (cart->prg.ram.size > 0)
			cart_map(&cart->prg, RAM, 0x6000, 0, 8);
	}
}

static void mmc2_chr_read(struct cart *cart, uint16_t addr)
{
	uint16_t u0 = 0x0FD8;
	uint16_t u1 = 0x0FE8;

	if (cart->hdr.mapper == 10) {
		u0 = 0x0FDF;
		u1 = 0x0FEF;
	}

	if (addr >= 0x0FD8 && addr <= u0) {
		cart_map(&cart->chr, ROM, 0x0000, cart->REG[0], 4);
	} else if (addr >= 0x0FE8 && addr <= u1) {
		cart_map(&cart->chr, ROM, 0x0000, cart->REG[1], 4);
	} else if (addr >= 0x1FD8 && addr <= 0x1FDF) {
		cart_map(&cart->chr, ROM, 0x1000, cart->REG[2], 4);
	} else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
		cart_map(&cart->chr, ROM, 0x1000, cart->REG[3], 4);
	}
}

static void mmc2_prg_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	if (cart->hdr.mapper == 10 && addr >= 0x6000 && addr < 0x8000 && cart->prg.ram.size > 0) {
		map_write(&cart->prg, 0, addr, v);
		cart->sram_dirty = cart->prg.sram;

	} else if (addr >= 0x8000) {
		switch (addr & 0xF000) {
			case 0xA000:
				cart_map(&cart->prg, ROM, 0x8000, v & 0x0F, cart->hdr.mapper == 10 ? 16 : 8);
				break;
			case 0xB000:
				cart->REG[0] = v & 0x1F;
				cart_map(&cart->chr, ROM, 0x0000, cart->REG[0], 4);
				break;
			case 0xC000:
				cart->REG[1] = v & 0x1F;
				cart_map(&cart->chr, ROM, 0x0000, cart->REG[1], 4);
				break;
			case 0xD000:
				cart->REG[2] = v & 0x1F;
				cart_map(&cart->chr, ROM, 0x1000, cart->REG[2], 4);
				break;
			case 0xE000:
				cart->REG[3] = v & 0x1F;
				cart_map(&cart->chr, ROM, 0x1000, cart->REG[3], 4);
				break;
			case 0xF000:
				cart_map_ciram(&cart->chr, (v & 0x01) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL);
				break;
		}
	}
}
