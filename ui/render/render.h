#pragma once

#include <stdint.h>

#include "SDL2/SDL.h"
#include "parsec-dso.h"

#include "mod.h"

struct render;

int32_t render_init(struct render **render_out, enum render_mode mode, void *window,
	bool vsync, uint32_t width, uint32_t height, enum sampler sampler);
void render_destroy(struct render **render_out);
void render_draw(struct render *render, int32_t w, int32_t h, uint32_t *pixels, uint32_t aspect);
enum ParsecStatus render_submit_parsec(struct render *render, ParsecDSO *parsec);
void render_present(struct render *render);
void render_set_sampler(struct render *render, enum sampler sampler);

void render_ui_init(struct render *render, SDL_Window *window, struct ui_cbs *cbs, void *opaque);
void render_ui_draw(struct render *render, SDL_Window *window, struct ui_props *props);
void render_ui_set_popup(struct render *render, char *message, int32_t timeout);
void render_ui_sdl_input(struct render *render, SDL_Event *event);
bool render_ui_block_keyboard(struct render *render);
