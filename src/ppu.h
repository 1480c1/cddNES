#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "cpu.h"

struct ppu;

/*** READ & WRITE ***/
uint8_t ppu_read(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr);
void ppu_write(struct ppu *ppu, struct cpu *cpu, struct cart *cart, uint16_t addr, uint8_t v);

/*** RUN ***/
uint8_t ppu_step(struct ppu *ppu, struct cpu *cpu, struct cart *cart, FRAME_CALLBACK new_frame, void *opaque);

/*** INIT & DESTROY ***/
void ppu_init(struct ppu **ppu_out);
void ppu_destroy(struct ppu **ppu_out);
void ppu_reset(struct ppu *ppu);
