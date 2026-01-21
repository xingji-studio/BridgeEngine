#include "render/draw.h"
#include "master/init.h"
#include "bapi.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

void bapi_engine_render_drawpixel(int x, int y, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_draw_pixel(x, y, c);
}

void bapi_engine_render_fillrect(int ax, int ay, int width, int height, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_fill_rect(ax, ay, width, height, c);
}

void bapi_engine_render_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_draw_triangle(x1, y1, x2, y2, x3, y3, c);
}
