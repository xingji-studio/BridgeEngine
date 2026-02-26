#include "text/text.h"
#include "master/init.h"
#include "bapi_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FONTS 8
#define FONT_PATH "text/font.ttf"

typedef struct {
    TTF_Font* font;
    float size;
    int in_use;
} cached_font_t;

static cached_font_t g_font_cache[MAX_FONTS] = {0};
static int g_text_initialized = 0;

static TTF_Font* get_or_load_font(float size) {
    for (int i = 0; i < MAX_FONTS; i++) {
        if (g_font_cache[i].in_use && g_font_cache[i].size == size) {
            return g_font_cache[i].font;
        }
    }
    
    for (int i = 0; i < MAX_FONTS; i++) {
        if (!g_font_cache[i].in_use) {
            g_font_cache[i].font = TTF_OpenFont(FONT_PATH, size);
            if (g_font_cache[i].font) {
                g_font_cache[i].size = size;
                g_font_cache[i].in_use = 1;
                return g_font_cache[i].font;
            }
            return NULL;
        }
    }
    
    return NULL;
}

void bapi_text_init(void) {
    if (g_text_initialized) return;
    memset(g_font_cache, 0, sizeof(g_font_cache));
    g_text_initialized = 1;
}

void bapi_text_cleanup(void) {
    for (int i = 0; i < MAX_FONTS; i++) {
        if (g_font_cache[i].in_use && g_font_cache[i].font) {
            TTF_CloseFont(g_font_cache[i].font);
            g_font_cache[i].font = NULL;
            g_font_cache[i].in_use = 0;
        }
    }
    g_text_initialized = 0;
}

void bapi_draw_text(const char* text, float x, float y, float size, bapi_color_t color) {
    if (!text || !text[0]) return;
    
    TTF_Font* font = get_or_load_font(size);
    if (!font) {
        SDL_Log("Failed to load font size %.0f", size);
        return;
    }
    
    SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (!surface) {
        SDL_Log("Failed to render text: %s", SDL_GetError());
        return;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(bapi_internal_renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    
    SDL_FRect dest_rect = {x, y, (float)surface->w, (float)surface->h};
    SDL_RenderTexture(bapi_internal_renderer, texture, NULL, &dest_rect);
    
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void bapi_get_text_size(const char* text, float size, float* width, float* height) {
    if (width) *width = 0;
    if (height) *height = 0;
    
    if (!text || !text[0]) return;
    
    TTF_Font* font = get_or_load_font(size);
    if (!font) return;
    
    int w = 0, h = 0;
    TTF_GetStringSize(font, text, strlen(text), &w, &h);
    
    if (width) *width = (float)w;
    if (height) *height = (float)h;
}
