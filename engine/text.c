#include "text/text.h"
#include <stdio.h>

static TTF_Font *font = NULL;
extern SDL_Renderer *renderer;

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

SDL_Texture* bapi_text_render(const char* text, int color) {
    if (font == NULL || text == NULL) {
        return NULL;
    }
    
    SDL_Color sdl_color = {
        (color >> 24) & 0xFF,    // R
        (color >> 16) & 0xFF,    // G
        (color >> 8) & 0xFF,     // B
        color & 0xFF             // A
    };
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (surface == NULL) {
        SDL_Log("Failed to render text: %s", SDL_GetError());
        return NULL;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    
    if (texture == NULL) {
        SDL_Log("Failed to create texture from text surface");
        return NULL;
    }
    
    return texture;
}

void bapi_text_draw(SDL_Texture* text_texture, float x, float y, float width, float height) {
    if (text_texture == NULL) {
        return;
    }
    
    SDL_FRect dest_rect = {x, y, width, height};
    SDL_RenderTexture(renderer, text_texture, NULL, &dest_rect);
}

void bapi_text_destroy(SDL_Texture* text_texture) {
    if (text_texture != NULL) {
        SDL_DestroyTexture(text_texture);
    }
}

void bapi_text_cleanup(void) {
    if (font != NULL) {
        TTF_CloseFont(font);
        font = NULL;
    }
    TTF_Quit();
}