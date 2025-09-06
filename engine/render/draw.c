#include "render/render.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "render/draw.h"

extern SDL_Renderer *renderer;

typedef struct rgba {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} rgba;

rgba bapi_engine_render_hex2rgba(int hex_color) {
    rgba color;
    color.r = (hex_color >> 24) & 0xFF;
    color.g = (hex_color >> 16) & 0xFF;
    color.b = (hex_color >> 8) & 0xFF;
    color.a = hex_color & 0xFF;
    return color;
}

void bapi_engine_render_drawpixel(float x, float y, int color) {
    rgba c = bapi_engine_render_hex2rgba(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderPoint(renderer, x, y);
}

void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color) {
    rgba c = bapi_engine_render_hex2rgba(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_FRect rect = {ax, ay, width, height};
    SDL_RenderFillRect(renderer, &rect);
}

void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color) {
    rgba c = bapi_engine_render_hex2rgba(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    
    SDL_RenderLine(renderer, x1, y1, x2, y2);
    SDL_RenderLine(renderer, x2, y2, x3, y3);
    SDL_RenderLine(renderer, x3, y3, x1, y1);
}

SDL_Texture* bapi_engine_render_load_image(const char* filepath) {
    SDL_Surface* surface = IMG_Load(filepath);
    if (surface == NULL) {
        SDL_Log("Failed to load image %s: %s\n", filepath, SDL_GetError());
        return NULL;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    
    if (texture == NULL) {
        SDL_Log("Failed to create texture from %s: %s\n", filepath, SDL_GetError());
        return NULL;
    }
    
    return texture;
}

void bapi_engine_render_draw_image(SDL_Texture* texture, float x, float y, float width, float height) {
    if (texture == NULL) {
        SDL_Log("Texture is NULL\n");
        return;
    }
    
    SDL_FRect destRect;
    destRect.x = x;
    destRect.y = y;
    destRect.w = width;
    destRect.h = height;
    
    SDL_RenderTexture(renderer, texture, NULL, &destRect);
}

void bapi_engine_render_destroy_image(SDL_Texture* texture) {
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
}