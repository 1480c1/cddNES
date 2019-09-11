// https://wiki.nesdev.com/w/index.php/MMC3

static void mmc3_map_prg(struct cart *cart)
{
	uint16_t b0 = cart->REG[6];
	uint16_t b1 = (uint16_t) (cart->prg.rom.size / 0x2000) - 2;

	cart_map(&cart->prg, ROM, 0x8000, cart->prg_mode == 0 ? b0 : b1, 8);
	cart_map(&cart->prg, ROM, 0xA000, cart->REG[7], 8);
	cart_map(&cart->prg, ROM, 0xC000, cart->prg_mode == 0 ? b1 : b0, 8);
}

static void mmc3_map_chr(struct cart *cart)
{
	enum mem type = cart->chr.rom.size > 0 ? ROM : RAM;
	uint16_t o1 = cart->chr_mode == 0 ? 0 : 0x1000;
	uint16_t o2 = cart->chr_mode == 0 ? 4 : 0;

	cart_map(&cart->chr, type, o1 + 0x0000, cart->REG[0] >> 1, 2);
	cart_map(&cart->chr, type, o1 + 0x0800, cart->REG[1] >> 1, 2);

	for (uint8_t x = 0; x < 4; x++)
		cart_map(&cart->chr, type, (o2 + x) * 0x0400, cart->REG[2 + x], 1);
}

static void mmc3_init(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x2000) - 1;

	cart_map(&cart->prg, ROM, 0xE000, last_bank, 8);

	cart->REG[6] = 0;
	cart->REG[7] = 1;
	mmc3_map_prg(cart);

	if (cart->prg.ram.size > 0)
		cart_map(&cart->prg, RAM, 0x6000, 0, 8);
}

static void mmc3_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v)
{
	if (addr >= 0x6000 && addr < 0x8000) {
		map_write(&cart->prg, 0, addr, v);
		cart->sram_dirty = cart->prg.sram;

	} else {
		switch (addr & 0xE001) {
			case 0x8000:
				cart->mmc3.bank_update = v & 0x07;
				cart->prg_mode = (v & 0x40) >> 6;
				cart->chr_mode = (v & 0x80) >> 7;
				mmc3_map_chr(cart);
				mmc3_map_prg(cart);
				break;
			case 0x8001:
				cart->REG[cart->mmc3.bank_update] = v;

				if (cart->mmc3.bank_update < 6) {
					mmc3_map_chr(cart);

				} else {
					mmc3_map_prg(cart);
				}
				break;
			case 0xA000:
				if (cart->hdr.mirroring != MIRROR_FOUR)
					cart_map_ciram(&cart->chr, (v & 0x01) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL);
				break;
			case 0xA001: // RAM protect
				nes_log("MMC3 RAM protect: %x", v);
				break;
			case 0xC000:
				cart->irq.period = v;
				break;
			case 0xC001:
				cart->irq.reload = true;
				break;
			case 0xE000:
				cpu_irq(cpu, IRQ_MAPPER, false);
				cart->irq.enable = false;
				break;
			case 0xE001:
				cart->irq.enable = true;
				break;
			default:
				nes_log("Uncaught MMC3 write %X: %X", addr, v);
				break;
		}
	}
}

static void mmc3_ppu_a12_toggle(struct cart *cart)
{
	cart->irq.pending = true;
}

static void mmc3_step(struct cart *cart, struct cpu *cpu)
{
	if (cart->irq.pending) {
		bool set_irq = true;

		if (cart->irq.counter == 0 || cart->irq.reload) {
			if (cart->hdr.nes2.submapper == 4 || cart->hdr.nes2.submapper == 1)
				set_irq = cart->irq.reload;

			cart->irq.reload = false;
			cart->irq.counter = cart->irq.period;

		} else {
			cart->irq.counter--;
		}

		if (set_irq && cart->irq.enable && cart->irq.counter == 0)
			cpu_irq(cpu, IRQ_MAPPER, true);

		cart->irq.pending = false;
	}
}
