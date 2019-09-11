#pragma once

#include <stdbool.h>

#include <SDL2/SDL.h>
#include "imgui.h"

IMGUI_IMPL_API bool ImGui_ImplcddSDL_Init(SDL_Window *window);
IMGUI_IMPL_API void ImGui_ImplcddSDL_Shutdown(void);
IMGUI_IMPL_API void ImGui_ImplcddSDL_NewFrame(SDL_Window *window);
IMGUI_IMPL_API bool ImGui_ImplcddSDL_ProcessEvent(const SDL_Event *event);
IMGUI_IMPL_API void ImGui_ImplcddSDL_ClearInput(void);
