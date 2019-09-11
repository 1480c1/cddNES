#pragma once

#include <stdbool.h>

#include "SDL2/SDL.h"

#include "mod.h"

struct ui;

#ifdef __cplusplus
extern "C" {
#endif

void ui_set_popup(struct ui *ctx, const char *message, int32_t timeout);
bool ui_block_keyboard(struct ui *ctx);
void ui_sdl_input(struct ui *ctx, SDL_Event *event);

void ui_init(struct ui **ctx_out, SDL_Window *window, struct ui_cbs *cbs, void *opaque,
	enum render_mode mode, struct render_device *device, struct render_context *context);
void ui_new_frame(struct ui *ctx, struct render_device *device, struct render_context *context,
	SDL_Window *window, struct ui_props *props);
void ui_destroy(struct ui **ctx_out);

#ifdef __cplusplus
}
#endif
