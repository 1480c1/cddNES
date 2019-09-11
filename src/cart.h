#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cpu.h"

struct cart;

/*** READ & WRITE ***/
uint8_t cart_prg_read(struct cart *cart, struct cpu *cpu, uint16_t addr, bool *mem_hit);
void cart_prg_write(struct cart *cart, struct cpu *cpu, uint16_t addr, uint8_t v);
uint8_t cart_chr_read(struct cart *cart, uint16_t addr, enum mem type, bool nt);
void cart_chr_write(struct cart *cart, uint16_t addr, uint8_t v);

/*** HOOKS ***/
void cart_ppu_a12_toggle(struct cart *cart);
void cart_ppu_write_hook(struct cart *cart, uint16_t addr, uint8_t v);
void cart_ppu_scanline_hook(struct cart *cart, struct cpu *cpu, uint16_t scanline);
bool cart_block_2007(struct cart *cart);

/*** RUN ***/
void cart_step(struct cart *cart, struct cpu *cpu);

/*** SRAM ***/
size_t cart_sram_dirty(struct cart *cart);
void cart_sram_get(struct cart *cart, uint8_t *buf, size_t size);

/*** INIT & DESTROY ***/
void cart_init(struct cart **cart_out, uint8_t *rom, size_t rom_len,
	uint8_t *sram, size_t sram_len, struct nes_header *hdr);
void cart_destroy(struct cart **cart_out);
