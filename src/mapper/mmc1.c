// https://wiki.nesdev.com/w/index.php/MMC1

static void mmc1_map_prg(struct cart *cart, uint8_t bank)
{
	//SEROM et al
	if (cart->hdr.nes2.submapper == 5)
		return;

	uint8_t offset = cart->mmc1.use256 ? 16 : 0;

	switch (cart->prg_mode) {
		case 0:
		case 1:
			cart_map(&cart->prg, ROM, 0x8000, (bank + offset) >> 1, 32);
			break;
		case 2:
			cart_map(&cart->prg, ROM, 0x8000, 0 + offset, 16);
			cart_map(&cart->prg, ROM, 0xC000, bank + offset, 16);
			break;
		case 3:
			cart_map(&cart->prg, ROM, 0x8000, bank + offset, 16);
			cart_map(&cart->prg, ROM, 0xC000, 15 + offset, 16);
			break;
	}
}

static void mmc1_map_chr(struct cart *cart, uint8_t slot, uint8_t bank)
{
	enum mem type = cart->chr.rom.size > 0 ? ROM : RAM;

	//SUROM et al
	if (type == RAM) {
		bool use256 = (bank & 0x10) ? true : false;

		if (cart->mmc1.use256 != use256) {
			cart->mmc1.use256 = use256;
			mmc1_map_prg(cart, cart->PRG[0]);
		}

		if (cart->prg.ram.size > 0x2000)
			cart_map(&cart->prg, RAM, 0x6000, (bank & 0x0C) >> 2, 8);

		bank &= 0x01;
	}

	switch (cart->chr_mode) {
		case 0:
			if (slot == 0)
				cart_map(&cart->chr, type, 0x0000, bank >> 1, 8);
			break;
		case 1:
			cart_map(&cart->chr, type, slot * 0x1000, bank, 4);
			break;
	}
}

static void mmc1_map_all(struct cart *cart)
{
	mmc1_map_prg(cart, cart->PRG[0]);
	mmc1_map_chr(cart, 0, cart->CHR[0]);
	mmc1_map_chr(cart, 1, cart->CHR[1]);
}

static void mmc1_init(struct cart *cart)
{
	cart->prg_mode = 3;
	cart->chr_mode = 0;
	cart->ram_enable = true;

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);

	mmc1_map_all(cart);
}

static void mmc1_prg_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	//battery backed sram
	if (addr >= 0x6000 && addr < 0x8000 && cart->ram_enable) {
		map_write(&cart->prg, 0, addr, v);
		cart->sram_dirty = cart->prg.sram;

	} else if (addr >= 0x8000) {
		if (cart->mmc1.cycle == cart->cycle - 1)
			return;

		cart->mmc1.cycle = cart->cycle;

		//high bit begins the sequence
		if (v & 0x80) {
			cart->mmc1.n = 0;
			cart->REG[0] = 0;
			mmc1_init(cart);

		// if high bit is not set, continue the sequence
		} else {
			cart->REG[0] |= (v & 0x01) << cart->mmc1.n;

			if (++cart->mmc1.n == 5) {
				switch ((addr >> 13) & 0x03) {
					case 0:
						cart->prg_mode = (cart->REG[0] & 0x0C) >> 2;
						cart->chr_mode = (cart->REG[0] & 0x10) >> 4;
						mmc1_map_all(cart);

						switch (cart->REG[0] & 0x03) {
							case 0: cart_map_ciram(&cart->chr, MIRROR_SINGLE0);    break;
							case 1: cart_map_ciram(&cart->chr, MIRROR_SINGLE1);    break;
							case 2: cart_map_ciram(&cart->chr, MIRROR_VERTICAL);   break;
							case 3: cart_map_ciram(&cart->chr, MIRROR_HORIZONTAL); break;
						}
						break;
					case 1:
						cart->CHR[0] = cart->REG[0] & 0x1F;
						mmc1_map_chr(cart, 0, cart->CHR[0]);
						break;
					case 2:
						cart->CHR[1] = cart->REG[0] & 0x1F;
						mmc1_map_chr(cart, 1, cart->CHR[1]);
						break;
					case 3:
						cart->ram_enable = !(cart->REG[0] & 0x10);
						cart->PRG[0] = cart->REG[0] & 0x0F;
						mmc1_map_prg(cart, cart->PRG[0]);
						break;
				}

				cart->mmc1.n = 0;
				cart->REG[0] = 0;
			}
		}
	}
}
