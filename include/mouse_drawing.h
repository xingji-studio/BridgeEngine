#pragma once

#include <SDL3/SDL.h>

void bapi_mouse_drawing_init(void);
void bapi_mouse_drawing_handle_event(SDL_Event *event);
void bapi_mouse_drawing_render(void);
void bapi_mouse_drawing_cleanup(void);
void bapi_mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color);
void bapi_mouse_drawing_clear_lines(void);