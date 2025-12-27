#include "text/text.h"
#include "master/init.h"
#include "bapi_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* font = NULL;

int bapi_text_init(const char* font_path, int font_size) {
    if (!TTF_Init()) {
        SDL_Log("Failed to initialize SDL_ttf");
        return -1;
    }

    font = TTF_OpenFont(font_path, font_size);
    if (font == NULL) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        TTF_Quit();
        return -1;
    }

    return 0;
}

bapi_texture_t bapi_render_text(const char* text, bapi_color_t color) {
    if (font == NULL || text == NULL) {
        return NULL;
    }

    SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (surface == NULL) {
        SDL_Log("Failed to render text: %s", SDL_GetError());
        return NULL;
    }

    SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(bapi_internal_renderer, surface);
    SDL_DestroySurface(surface);

    if (sdl_texture == NULL) {
        SDL_Log("Failed to create texture from text surface");
        return NULL;
    }

    bapi_texture_t texture = malloc(sizeof(bapi_texture_t));
    if (texture == NULL) {
        SDL_DestroyTexture(sdl_texture);
        return NULL;
    }
    texture->texture = sdl_texture;

    return texture;
}

void bapi_draw_text(bapi_texture_t text_texture, float x, float y, float w, float h) {
    if (text_texture == NULL || text_texture->texture == NULL) {
        return;
    }

    SDL_FRect dest_rect = {x, y, w, h};
    SDL_RenderTexture(bapi_internal_renderer, text_texture->texture, NULL, &dest_rect);
}

void bapi_destroy_text(bapi_texture_t text_texture) {
    if (text_texture != NULL) {
        if (text_texture->texture != NULL) {
            SDL_DestroyTexture(text_texture->texture);
        }
        free(text_texture);
    }
}

void bapi_text_cleanup(void) {
    if (font != NULL) {
        TTF_CloseFont(font);
        font = NULL;
    }
    TTF_Quit();
}
