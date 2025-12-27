#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void bapi_draw_pixel(float x, float y, bapi_color_t color);
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
bapi_texture_t bapi_load_image(const char* filepath);
void bapi_draw_image(bapi_texture_t texture, float x, float y, float w, float h);
void bapi_destroy_texture(bapi_texture_t texture);

#ifdef __cplusplus
}
#endif
