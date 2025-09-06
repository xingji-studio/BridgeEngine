#include "render/render.h"
#include <SDL3/SDL.h>
#include <math.h>

typedef struct rgba {
	int r;
	int g;
	int b;
	int a;
} rgba_t;

extern SDL_Renderer *renderer;

static rgba_t bapi_engine_render_hex2rgba(int color);

static rgba_t bapi_engine_render_hex2rgba(int color)
{
	rgba_t target;

	target.r = (color >> 24) & 0xFF;
	target.g = (color >> 16) & 0xFF;
	target.b = (color >> 8) & 0xFF;
	target.a = color & 0xFF;

	return target;
}

void bapi_engine_render_drawpixel(float x, float y, int color)
{
	rgba_t rgba_color = bapi_engine_render_hex2rgba(color);

	SDL_SetRenderDrawColor(renderer, rgba_color.r, rgba_color.g, rgba_color.b, rgba_color.a);

	// SDL_Log("Create pixel. x = %f y = %f color = %#x rgba = (%d, %d, %d, %d)\n", x, y, color,
	// 		rgba_color.r, rgba_color.g, rgba_color.b, rgba_color.a);

	SDL_RenderPoint(renderer, x, y);
}

void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color)
{
	rgba_t rgba_color = bapi_engine_render_hex2rgba(color);

	SDL_SetRenderDrawColor(renderer, rgba_color.r, rgba_color.g, rgba_color.b, rgba_color.a);

	SDL_FRect rect;
	rect.x = ax;
	rect.y = ay;
	rect.w = width;
	rect.h = height;

	// SDL_Log("Create rect. x = %f y = %f w = %f h = %f color = %#x rgba = (%d, %d, %d, %d)\n",
	// 		rect.x, rect.y, rect.w, rect.h, color, rgba_color.r, rgba_color.g, rgba_color.b,
	// 		rgba_color.a);

	SDL_RenderFillRect(renderer, &rect);
}


void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color) {
    if (y1 > y2) {
        float tempX = x1, tempY = y1;
        x1 = x2; y1 = y2;
        x2 = tempX; y2 = tempY;
    }
    if (y1 > y3) {
        float tempX = x1, tempY = y1;
        x1 = x3; y1 = y3;
        x3 = tempX; y3 = tempY;
    }
    if (y2 > y3) {
        float tempX = x2, tempY = y2;
        x2 = x3; y2 = y3;
        x3 = tempX; y3 = tempY;
    }

    int totalHeight = (int)(y3 - y1);
    for (int i = 0; i < totalHeight; i++) {
        bool secondHalf = i > (int)(y2 - y1) || y2 == y1;
        int segmentHeight = secondHalf ? (int)(y3 - y2) : (int)(y2 - y1);
        float alpha = (float)i / totalHeight;
        float beta = (float)(i - (secondHalf ? (y2 - y1) : 0)) / segmentHeight;
        float A_x = x1 + (x3 - x1) * alpha;
        float B_x = secondHalf ? x2 + (x3 - x2) * beta : x1 + (x2 - x1) * beta;
        if (A_x > B_x) {
            float temp = A_x;
            A_x = B_x;
            B_x = temp;
        }
        for (int j = (int)A_x; j <= (int)B_x; j++) {
            bapi_engine_render_drawpixel(j, (int)(y1 + i), color);
        }
    }
}