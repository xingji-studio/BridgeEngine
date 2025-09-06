#pragma once

#include <SDL3/SDL.h>

void bapi_engine_render_drawpixel(float x, float y, int color);
void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color);
void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color);
SDL_Texture* bapi_engine_render_load_image(const char* filepath);
void bapi_engine_render_draw_image(SDL_Texture* texture, float x, float y, float width, float height);
void bapi_engine_render_destroy_image(SDL_Texture* texture);