#pragma once

#include <stdbool.h>

#include "imgui/imgui.h"

#include "mod.h"

struct ui_d3d12_shim;

#ifdef __cplusplus
extern "C" {
#endif

void ui_d3d12_init(struct render_device *device, struct ui_d3d12_shim **shim_out);
void ui_d3d12_shutdown(struct ui_d3d12_shim **shim_out);
void ui_d3d12_new_frame(void);
void ui_d3d12_render_draw_data(struct ui_d3d12_shim *shim, struct render_context *context, ImDrawData* draw_data);

#ifdef __cplusplus
}
#endif
