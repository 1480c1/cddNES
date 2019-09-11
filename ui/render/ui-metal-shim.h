#pragma once

#include <stdbool.h>

#include "imgui/imgui.h"

#include "mod.h"

bool ui_metal_init(struct render_device *device);
void ui_metal_shutdown(void);
void ui_metal_new_frame(struct render_context *context);
void ui_metal_render_draw_data(ImDrawData* draw_data, struct render_device *device, struct render_context *context);
