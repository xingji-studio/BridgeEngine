#pragma once

#include <SDL3/SDL.h>

void mouse_drawing_init(void);
void mouse_drawing_handle_event(SDL_Event *event);
void mouse_drawing_render(void);
void mouse_drawing_cleanup(void);
