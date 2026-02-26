#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void bapi_draw_pixel(float x, float y, bapi_color_t color);
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);

#ifdef __cplusplus
}
#endif
