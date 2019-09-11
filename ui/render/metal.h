#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "parsec-dso.h"

#include "mod.h"

int32_t mtl_init(struct render_mod **mod_out, void *window, bool vsync,
	uint32_t width, uint32_t height, enum sampler sampler);
void mtl_destroy(struct render_mod **mod_out);
void mtl_get_device(struct render_mod *mod, struct render_device **device, struct render_context **context);
void mtl_draw(struct render_mod *mod, uint32_t window_w, uint32_t window_h, uint32_t *pixels, uint32_t aspect);
enum ParsecStatus mtl_submit_parsec(struct render_mod *mod, ParsecDSO *parsec);
void mtl_present(struct render_mod *mod);
void mtl_set_sampler(struct render_mod *mod, enum sampler sampler);
