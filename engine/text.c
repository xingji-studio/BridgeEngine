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

    font = TTF_OpenFont("text/font.ttf", size);
    if (font == NULL) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        TTF_Quit();
        return;
    }

    if (font == NULL || text == NULL) {
        return;
    }

    SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (surface == NULL) {
        SDL_Log("Failed to render text: %s", SDL_GetError());
        return;
    }

    SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(bapi_internal_renderer, surface);

    if (sdl_texture == NULL) {
        SDL_Log("Failed to create texture from text surface");
        return;
    }

    bapi_texture_t texture = malloc(sizeof(bapi_texture_t));
    if (texture == NULL) {
        SDL_DestroyTexture(sdl_texture);
        return;
    }
    texture->texture = sdl_texture;

    if (texture == NULL || texture->texture == NULL) {
        return;
    }

    SDL_FRect dest_rect = {x, y, surface->w, surface->h};
    SDL_RenderTexture(bapi_internal_renderer, texture->texture, NULL, &dest_rect);

    SDL_DestroySurface(surface);

    if (texture != NULL) {
        if (texture->texture != NULL) {
            SDL_DestroyTexture(texture->texture);
        }
        free(texture);
    }

    if (font != NULL) {
        TTF_CloseFont(font);
        font = NULL;
    }
}
