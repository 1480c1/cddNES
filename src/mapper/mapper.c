struct mapper {
	uint16_t reg_low;
	uint16_t reg_high;
	uint8_t prg_size;
	uint8_t chr_size;
	uint8_t chr_slots;
	uint16_t prg_addr;
	uint8_t prg_mask;
	uint8_t prg_shift;
	uint8_t chr0_mask;
	int8_t chr0_shift;
	uint8_t chr1_mask;
	int8_t chr1_shift;
	bool chr_combine;
	uint8_t mirror_table;
	uint8_t mirror_mask;
	uint8_t mirror_shift;
};

static enum mirror MIRROR[6][4] = {
	{0, 0},
	{MIRROR_SINGLE1,    MIRROR_SINGLE0},
	{MIRROR_SINGLE0,    MIRROR_SINGLE1},
	{MIRROR_HORIZONTAL, MIRROR_VERTICAL},
	{MIRROR_SINGLE0,    MIRROR_HORIZONTAL,
	 MIRROR_VERTICAL,   MIRROR_SINGLE1},
	{MIRROR_FOUR8,      MIRROR_FOUR16},
};

static struct mapper M[256] = {
	[0]   = {0x8000, 0xFFFF,  0, 0, 0,      0,    0, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[2]   = {0x8000, 0xFFFF, 16, 0, 0, 0x8000, 0xFF, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[3]   = {0x8000, 0xFFFF,  0, 8, 1,      0,    0, 0, 0x03, 0,    0,  0, false, 0,    0, 0},
	[7]   = {0x8000, 0xFFFF, 32, 0, 0, 0x8000, 0x0F, 0,    0, 0,    0,  0, false, 1, 0x10, 4},
	[11]  = {0x8000, 0xFFFF, 32, 8, 1, 0x8000, 0x03, 0, 0xF0, 4,    0,  0, false, 0,    0, 0},
	[13]  = {0x8000, 0xFFFF,  0, 4, 2,      0,    0, 0, 0x00, 0, 0x03,  0, false, 0,    0, 0},
	[30]  = {0x8000, 0xFFFF, 16, 8, 1, 0x8000, 0x1F, 0, 0x60, 5,    0,  0, false, 0,    0, 0},
	[31]  = {0x5000, 0x5FFF,  4, 0, 0,      0, 0xFF, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[34]  = {0x8000, 0xFFFF, 32, 4, 0, 0x8000, 0x0F, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[38]  = {0x7000, 0x7FFF, 32, 8, 1, 0x8000, 0x03, 0, 0x0C, 2,    0,  0, false, 0,    0, 0},
	[66]  = {0x8000, 0xFFFF, 32, 8, 1, 0x8000, 0x30, 4, 0x03, 0,    0,  0, false, 0,    0, 0},
	[70]  = {0x8000, 0xFFFF, 16, 8, 1, 0x8000, 0xF0, 4, 0x0F, 0,    0,  0, false, 0,    0, 0},
	[71]  = {0xC000, 0xFFFF, 16, 0, 0, 0x8000, 0x0F, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[77]  = {0x8000, 0xFFFF, 32, 2, 1, 0x8000, 0x0F, 0, 0xF0, 4,    0,  0, false, 0,    0, 0},
	[78]  = {0x8000, 0xFFFF, 16, 8, 1, 0x8000, 0x07, 0, 0xF0, 4,    0,  0, false, 3, 0x08, 3},
	[79]  = {0xE100, 0x4100, 32, 8, 1, 0x8000, 0x08, 3, 0x07, 0,    0,  0, false, 0,    0, 0},
	[87]  = {0x6000, 0x7FFF,  0, 8, 1,      0,    0, 0, 0x02, 1, 0x01, -1,  true, 0,    0, 0},
	[89]  = {0x8000, 0xFFFF, 16, 8, 1, 0x8000, 0x70, 4, 0x80, 4, 0x07,  0,  true, 2, 0x08, 3},
	[93]  = {0x8000, 0xFFFF, 16, 0, 0, 0x8000, 0x70, 4,    0, 0,    0,  0, false, 0,    0, 0},
	[94]  = {0x8000, 0xFFFF, 16, 0, 0, 0x8000, 0x1C, 2,    0, 0,    0,  0, false, 0,    0, 0},
	[97]  = {0x8000, 0xFFFF, 16, 0, 0, 0xC000, 0x0F, 0,    0, 0,    0,  0, false, 4, 0xC0, 6},
	[101] = {0x6000, 0x7FFF,  0, 8, 1,      0,    0, 0, 0xFF, 0,    0,  0, false, 0,    0, 0},
	[107] = {0x8000, 0xFFFF, 32, 8, 1, 0x8000, 0xFE, 1, 0xFF, 0,    0,  0, false, 0,    0, 0},
	[111] = {0x5000, 0x5FFF, 32, 8, 1, 0x8000, 0x0F, 0, 0x10, 4,    0,  0, false, 5, 0x20, 5},
	[113] = {0xE100, 0x4100, 32, 8, 1, 0x8000, 0x38, 3, 0x40, 3, 0x07,  0,  true, 3, 0x80, 7},
	[140] = {0x6000, 0x7FFF, 32, 8, 1, 0x8000, 0x30, 4, 0x0F, 0,    0,  0, false, 0,    0, 0},
	[145] = {0xE100, 0x4100,  0, 8, 1,      0,    0, 0, 0x80, 7,    0,  0, false, 0,    0, 0},
	[146] = {0xE100, 0x4100, 32, 8, 1, 0x8000, 0x08, 3, 0x07, 0,    0,  0, false, 0,    0, 0},
	[148] = {0x8000, 0xFFFF, 32, 8, 1, 0x8000, 0x08, 3, 0x07, 0,    0,  0, false, 0,    0, 0},
	[149] = {0x8000, 0xFFFF,  0, 8, 1,      0,    0, 0, 0x80, 7,    0,  0, false, 0,    0, 0},
	[152] = {0x8000, 0xFFFF, 16, 8, 1, 0x8000, 0x70, 4, 0x0F, 0,    0,  0, false, 2, 0x80, 7},
	[180] = {0x8000, 0xFFFF, 16, 0, 0, 0xC000, 0x0F, 0,    0, 0,    0,  0, false, 0,    0, 0},
	[184] = {0x6000, 0x7FFF,  0, 4, 2,      0,    0, 0, 0x03, 0, 0x70,  4, false, 0,    0, 0},
};

static void mapper_init(struct cart *cart)
{
	uint16_t last_bank = (uint16_t) (cart->prg.rom.size / 0x4000) - 1;

	// UxROM style 16K bank setup
	switch (cart->hdr.mapper) {
		case 2:
		case 30:
		case 70:
		case 71:
		case 78:
		case 89:
		case 93:
		case 94:
		case 152:
		case 180:
			cart_map(&cart->prg, ROM, 0x8000, 0, 16);
			cart_map(&cart->prg, ROM, 0xC000, last_bank, 16);
			break;
		case 77:
			cart_map(&cart->chr, RAM, 0x0800, 0, 6);
			break;
		case 97:
			cart_map(&cart->prg, ROM, 0x8000, last_bank, 16);
			cart_map(&cart->prg, ROM, 0xC000, 0, 16);
			break;
		case 31:
			cart_map(&cart->prg, ROM, 0xF000, 0xFF, 4);
			break;
	}

	// default mirroring
	if (M[cart->hdr.mapper].mirror_table > 0)
		cart_map_ciram(&cart->chr, MIRROR[M[cart->hdr.mapper].mirror_table][0]);

	// Holy Diver vs. that other game
	if (cart->hdr.mapper == 78 && cart->hdr.nes2.submapper == 1)
		M[78].mirror_table = 2;

	//BNROM vs. NINA-001
	if (cart->hdr.mapper == 34 && cart->chr.rom.size > 8) {
		M[34].reg_low = 0x7FFD;
		M[34].reg_high = 0x7FFF;
		M[34].prg_mask = 0x01;
		M[34].chr0_mask = 0x0F;
		M[34].chr1_mask = 0x0F;
	}

	// default SRAM
	switch (cart->hdr.mapper) {
		case 34:
			if (cart->prg.ram.size > 0)
				cart_map(&cart->prg, RAM, 0x6000, 0, 8);
			break;
	}
}

static void mapper_prg_write(struct cart *cart, uint16_t addr, uint8_t v)
{
	if (cart->hdr.nes2.submapper == 2)
		v = cart_bus_conflict(&cart->prg, addr, v);

	struct mapper *m = &M[cart->hdr.mapper];

	bool addr_match = (addr >= m->reg_low && addr <= m->reg_high) || (addr & m->reg_low) == m->reg_high;
	uint8_t chr_start = 0;

	uint16_t chr_bank[2];
	chr_bank[0] = (v & m->chr0_mask) >> m->chr0_shift;
	chr_bank[1] = m->chr1_shift > 0 ? (v & m->chr1_mask) >> m->chr1_shift :
		(v & m->chr1_mask) << abs(m->chr1_shift);

	if (m->chr_combine)
		chr_bank[0] |= chr_bank[1];

	switch (cart->hdr.mapper) {
		case 0:
			if (addr >= 0x6000 && addr < 0x8000 && cart->hdr.battery) {
				cart_map(&cart->prg, RAM, 0x6000, 0, 8);
				map_write(&cart->prg, 0, addr, v);
			}
			break;
		case 30:
			if (addr_match && (v & 0x80))
				cart_map_ciram(&cart->chr, MIRROR_SINGLE0);
			break;
		case 31:
			m->prg_addr = 0x8000 + ((addr & 0x07) * 0x1000);
			break;
		case 34:
			if (cart->prg.ram.size > 0 && addr >= 0x6000 && addr < 0x7FFD) {
				map_write(&cart->prg, 0, addr, v);

			} else if (addr_match) {
				switch (addr) {
					case 0x7FFD:
						m->prg_size = 32;
						m->chr_slots = 0;
						break;
					case 0x7FFE:
						m->prg_size = 0;
						m->chr_slots = 1;
						break;
					case 0x7FFF:
						m->prg_size = 0;
						m->chr_slots = 2;
						chr_start = 1;
						break;
				}
			}
			break;
		case 71:
			if (addr == 0x9000)
				cart_map_ciram(&cart->chr, (v & 0x10) ? MIRROR_SINGLE1 : MIRROR_SINGLE0);
			break;
		case 111:
			if (addr >= 0x7000 && addr <= 0x7FFF)
				addr_match = true;
			break;
		case 184:
			chr_bank[1] |= 0x04;
			break;
	}

	if (addr_match) {
		if (m->prg_size > 0)
			cart_map(&cart->prg, ROM, m->prg_addr, (v & m->prg_mask) >> m->prg_shift, m->prg_size);

		for (uint8_t x = chr_start; x < m->chr_slots; x++)
			cart_map(&cart->chr, cart->chr.rom.size > 0 ? ROM : RAM, x * 0x1000, chr_bank[x], m->chr_size);

		if (m->mirror_table > 0)
			cart_map_ciram(&cart->chr, MIRROR[m->mirror_table][(v & m->mirror_mask) >> m->mirror_shift]);
	}
}

static bool mapper_block_2007(struct cart *cart)
{
	return cart->read_counter++ < 2;
}
