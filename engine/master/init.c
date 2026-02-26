#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "bapi_types.h"
#include "bapi_internal.h"
#include "log/log.h"

struct bapi_window_internal {
    SDL_Window* window;
};

struct bapi_renderer_internal {
    SDL_Renderer* renderer;
};

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static bool initialized = false;

SDL_Renderer* bapi_internal_renderer = NULL;

bapi_window_t bapi_engine_get_window(void) {
    bapi_window_t win = malloc(sizeof(struct bapi_window_internal));
    if (win) {
        win->window = window;
    }
    return win;
}

bapi_renderer_t bapi_engine_get_renderer(void) {
    bapi_renderer_t rend = malloc(sizeof(struct bapi_renderer_internal));
    if (rend) {
        rend->renderer = renderer;
    }
    return rend;
}

int bapi_engine_init(const char* title, int width, int height)
{
    if (initialized) {
        return 0;
    }

    // BAPI_LOG_INFO("Initializing BridgeEngine with title='%s', width=%d, height=%d",
    //               title, width, height);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        BAPI_LOG_INIT_DEFAULT();
        BAPI_LOG_CRITICAL("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // BAPI_LOG_INFO("SDL initialized successfully");

    window = SDL_CreateWindow(title, width, height, 0);

    if (window == NULL) {
        BAPI_LOG_INIT_DEFAULT();
        BAPI_LOG_CRITICAL("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // BAPI_LOG_INFO("Window created successfully");

    renderer = SDL_CreateRenderer(window, NULL);
    bapi_internal_renderer = renderer;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderPresent(renderer);

    if (!TTF_Init()) {
        SDL_Log("Failed to initialize SDL_ttf");
        return 0;
    }

    initialized = true;
    return 0;
}

void bapi_engine_quit(void) {
    // BAPI_LOG_INFO("Shutting down BridgeEngine...");

    TTF_Quit();
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
    initialized = false;
    bapi_log_shutdown();
}

SDL_Renderer* bapi_get_internal_renderer(void) {
    return renderer;
}

int bapi_poll_event(bapi_event_t* event) {
    if (event == NULL) {
        return SDL_PollEvent(NULL);
    }
    return SDL_PollEvent(&event->event);
}

int bapi_event_get_type(const bapi_event_t* event) {
    return event->event.type;
}

uint8_t bapi_sdlkeycode_convert_table[0x80] = {
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\b', KEY_TAB, ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', KEY_ESC, ' ', ' ', ' ', ' ',  
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', 
    '@', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '[', '\\', ']', '^', '_', 
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', ' ',
};

uint8_t bapi_sdlspkeycode_convert_table[0x90] = {
    KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, ' ', KEY_SCROLL, ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', KEY_NUML, '/', '*', '-', '+',  
    '\n', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
    ' ', ' ', ' ', ' ', ' ', KEY_CTRL, KEY_SHIFT, KEY_ALT, ' ', KEY_CTRL, KEY_SHIFT, KEY_ALT, ' ', ' ', ' ', ' ', 
};

uint8_t bapi_event_get_key_code(const bapi_event_t* event) {
    if (event->event.key.key < 0x80)
        return bapi_sdlkeycode_convert_table[event->event.key.key];
    else
        return bapi_sdlspkeycode_convert_table[event->event.key.key - 0x40000039u];
}

int bapi_event_get_mouse_x(const bapi_event_t* event) {
    return event->event.button.x;
}

int bapi_event_get_mouse_y(const bapi_event_t* event) {
    return event->event.button.y;
}

int bapi_event_get_mouse_button(const bapi_event_t* event) {
    return event->event.button.button;
}

int bapi_event_get_motion_x(const bapi_event_t* event) {
    return event->event.motion.x;
}

int bapi_event_get_motion_y(const bapi_event_t* event) {
    return event->event.motion.y;
}

int bapi_event_is_mouse_button_down(const bapi_event_t* event) {
    return event->event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
}

int bapi_event_is_mouse_button_up(const bapi_event_t* event) {
    return event->event.type == SDL_EVENT_MOUSE_BUTTON_UP;
}

int bapi_event_is_mouse_motion(const bapi_event_t* event) {
    return event->event.type == SDL_EVENT_MOUSE_MOTION;
}

void bapi_render_clear(void) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void bapi_render_present(void) {
    SDL_RenderPresent(renderer);
}

void bapi_set_render_color(bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void bapi_delay(uint32_t ms) {
    SDL_Delay(ms);
}

bapi_color_t bapi_color_from_hex(uint32_t hex_color) {
    bapi_color_t color;
    color.r = (hex_color >> 24) & 0xFF;
    color.g = (hex_color >> 16) & 0xFF;
    color.b = (hex_color >> 8) & 0xFF;
    color.a = hex_color & 0xFF;
    return color;
}

bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    bapi_color_t color = {r, g, b, a};
    return color;
}

uint32_t bapi_get_ticks(void) {
    return SDL_GetTicks();
}

void bapi_draw_pixel(float x, float y, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoint(renderer, x, y);
}

void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer, x1, y1, x2, y2);
}

void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderRect(renderer, &rect);
}

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer, x1, y1, x2, y2);
    SDL_RenderLine(renderer, x2, y2, x3, y3);
    SDL_RenderLine(renderer, x3, y3, x1, y1);
}

void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int segments = 64;
    float angle_step = 2.0f * (float)M_PI / segments;
    
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angle_step;
        float angle2 = (i + 1) * angle_step;
        
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int segments = 64;
    float angle_step = 2.0f * (float)M_PI / segments;
    
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angle_step;
        float angle2 = (i + 1) * angle_step;
        
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        
        SDL_RenderLine(renderer, cx, cy, x1, y1);
        SDL_RenderLine(renderer, x1, y1, x2, y2);
        SDL_RenderLine(renderer, x2, y2, cx, cy);
    }
}

void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color) {
    if (sides < 3) {
        return;
    }
    
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float angle_step = 2.0f * (float)M_PI / sides;
    
    for (int i = 0; i < sides; i++) {
        float angle1 = i * angle_step - (float)M_PI / 2;
        float angle2 = (i + 1) * angle_step - (float)M_PI / 2;
        
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color) {
    if (sides < 3) {
        return;
    }
    
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float angle_step = 2.0f * (float)M_PI / sides;
    
    for (int i = 0; i < sides; i++) {
        float angle1 = i * angle_step - (float)M_PI / 2;
        float angle2 = (i + 1) * angle_step - (float)M_PI / 2;
        
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        
        SDL_RenderLine(renderer, cx, cy, x1, y1);
        SDL_RenderLine(renderer, x1, y1, x2, y2);
        SDL_RenderLine(renderer, x2, y2, cx, cy);
    }
}

void bapi_draw_image(const char* filepath, float x, float y, float w, float h) {
    SDL_Surface* surface = IMG_Load(filepath);
    if (surface == NULL) {
        SDL_Log("Failed to load image %s: %s\n", filepath, SDL_GetError());
        return;
    }

    bapi_texture_t texture = malloc(sizeof(struct bapi_texture_internal));
    if (texture == NULL) {
        SDL_DestroySurface(surface);
        return;
    }

    texture->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (texture->texture == NULL) {
        SDL_Log("Failed to create texture from %s: %s\n", filepath, SDL_GetError());
        free(texture);
        return;
    }

    if (texture == NULL || texture->texture == NULL) {
        SDL_Log("Texture is NULL\n");
        return;
    }

    SDL_FRect destRect = {x, y, w, h};
    SDL_RenderTexture(renderer, texture->texture, NULL, &destRect);

    if (texture != NULL) {
        if (texture->texture != NULL) {
            SDL_DestroyTexture(texture->texture);
        }
        free(texture);
    }
}