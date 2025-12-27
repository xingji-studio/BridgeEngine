#pragma once

#include <SDL3/SDL.h>

#define BAPI_EVENT_QUIT SDL_EVENT_QUIT
#define BAPI_EVENT_KEY_DOWN SDL_EVENT_KEY_DOWN
#define BAPI_EVENT_MOUSE_BUTTON_DOWN SDL_EVENT_MOUSE_BUTTON_DOWN
#define BAPI_EVENT_MOUSE_BUTTON_UP SDL_EVENT_MOUSE_BUTTON_UP
#define BAPI_EVENT_MOUSE_MOTION SDL_EVENT_MOUSE_MOTION
#define BAPI_BUTTON_LEFT SDL_BUTTON_LEFT

struct bapi_event_internal {
    SDL_Event event;
};

typedef struct bapi_event_internal bapi_event_t;

extern SDL_Renderer* bapi_internal_renderer;

uint32_t bapi_get_ticks(void);
