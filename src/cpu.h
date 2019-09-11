#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "nes.h"

enum irq {
	IRQ_APU    = 0x01,
	IRQ_DMC    = 0x02,
	IRQ_MAPPER = 0x04,
};

struct cpu;

/*** INTERRUPTS ***/
void cpu_irq(struct cpu *cpu, enum irq irq, bool enabled);
void cpu_nmi(struct cpu *cpu, bool enabled);

/*** DMA ***/
void cpu_dma_oam(struct cpu *cpu, struct nes *nes, uint8_t v, bool odd_cycle);
uint8_t cpu_dma_dmc(struct cpu *cpu, struct nes *nes, uint16_t addr, bool in_write, bool begin_oam);

/*** RUN ***/
void cpu_step(struct cpu *cpu, struct nes *nes);

/*** INIT & DESTROY ***/
void cpu_init(struct cpu **cpu_out);
void cpu_destroy(struct cpu **cpu_out);
void cpu_reset(struct cpu *cpu, struct nes *nes, bool hard);
