#include "text/text.h"
#include "master/init.h"
#include "bapi_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bapi_draw_text(const char* text, float x, float y, float size, bapi_color_t color) {
    static void* font = NULL;

    if (text == NULL) {
        return;
    }

    font = TTF_OpenFont("text/font.ttf", size);
    if (font == NULL) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        return;
    }

    SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (surface == NULL) {
        SDL_Log("Failed to render text: %s", SDL_GetError());
        TTF_CloseFont(font);
        return;
    }

    SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(bapi_internal_renderer, surface);

    if (sdl_texture == NULL) {
        SDL_Log("Failed to create texture from text surface");
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        return;
    }

    SDL_FRect dest_rect = {x, y, surface->w, surface->h};
    SDL_RenderTexture(bapi_internal_renderer, sdl_texture, NULL, &dest_rect);

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroySurface(surface);
    TTF_CloseFont(font);
}

void bapi_get_text_size(const char* text, float size, float* width, float* height) {
    static void* font = NULL;

    if (text == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    font = TTF_OpenFont("text/font.ttf", size);
    if (font == NULL) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    int text_w, text_h;
    if (TTF_GetStringSize(font, text, -1, &text_w, &text_h) != 0) {
        SDL_Log("Failed to measure text: %s", SDL_GetError());
        if (width) *width = 0;
        if (height) *height = 0;
        TTF_CloseFont(font);
        return;
    } else {
        if (width) *width = (float)text_w;
        if (height) *height = (float)text_h;
    }

    TTF_CloseFont(font);
}
