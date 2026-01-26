#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_window_internal* bapi_window_t;
typedef struct bapi_renderer_internal* bapi_renderer_t;
typedef struct bapi_texture_internal* bapi_texture_t;

struct bapi_texture_internal {
    void* texture;
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} bapi_color_t;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} bapi_rect_t;

typedef struct {
    bapi_rect_t rect;
    bapi_color_t normal_color;
    bapi_color_t hover_color;
    bapi_color_t click_color;
    const char* text;
    bapi_color_t text_color;
    float text_size;
    float text_width;
    float text_height;
    int is_clicked;
    int is_hovered;
} bapi_button_t;

enum special_key_code
{
    KEY_ESC = 128,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_ENTER,
    KEY_CAPS,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_SPACE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_NUML,
    KEY_SCROLL
};

#ifdef __cplusplus
}
#endif
