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

#ifdef __cplusplus
}
#endif
