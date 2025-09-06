#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

int bapi_text_init(const char* font_path, int font_size);
SDL_Texture* bapi_text_render(const char* text, int color);
void bapi_text_draw(SDL_Texture* text_texture, float x, float y, float width, float height);
void bapi_text_destroy(SDL_Texture* text_texture);
void bapi_text_cleanup(void);