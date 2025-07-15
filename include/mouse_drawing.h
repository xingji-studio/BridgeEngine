#pragma once

#include <SDL3/SDL.h>

void mouse_drawing_init(void);
void mouse_drawing_handle_event(SDL_Event *event);
void mouse_drawing_render(void);
void mouse_drawing_cleanup(void);
void mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color);