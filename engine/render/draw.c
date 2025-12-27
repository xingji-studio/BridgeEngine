#include "render/draw.h"
#include "master/init.h"
#include "bapi.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

void bapi_engine_render_drawpixel(float x, float y, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_draw_pixel(x, y, c);
}

void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_fill_rect(ax, ay, width, height, c);
}

void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_draw_triangle(x1, y1, x2, y2, x3, y3, c);
}

bapi_texture_t bapi_engine_render_load_image(const char* filepath) {
    return bapi_load_image(filepath);
}

void bapi_engine_render_draw_image(bapi_texture_t texture, float x, float y, float width, float height) {
    bapi_draw_image(texture, x, y, width, height);
}

void bapi_engine_render_destroy_image(bapi_texture_t texture) {
    bapi_destroy_texture(texture);
}
