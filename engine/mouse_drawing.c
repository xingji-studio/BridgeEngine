#include "mouse_drawing.h"
#include "render/render.h"
#include <SDL3/SDL.h>

extern SDL_Renderer *renderer;

static bool isDrawing = false;
static int lastX, lastY;

void mouse_drawing_init(void) {
    isDrawing = false;
}

void mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color) {
    float dx = fabsf(x2 - x1);
    float dy = fabsf(y2 - y1);
    float sx = (x1 < x2) ? 1.0f : -1.0f;
    float sy = (y1 < y2) ? 1.0f : -1.0f;
    float err = dx - dy;

    while (1) {
        engine_render_drawpixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        float e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void mouse_drawing_handle_event(SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            isDrawing = true;
            lastX = event->button.x;
            lastY = event->button.y;
        }
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            isDrawing = false;
        }
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (isDrawing) {
            int currentX = event->motion.x;
            int currentY = event->motion.y;
            mouse_drawing_draw_line((float)lastX, (float)lastY, 
                                  (float)currentX, (float)currentY, 0xffffffff);
            lastX = currentX;
            lastY = currentY;
        }
    }
}

void mouse_drawing_render(void) {
    // 这里可以添加额外的渲染逻辑，如果有的话:)
}

void mouse_drawing_cleanup(void) {
    // 这里可以添加清理资源的逻辑，如果有的话:)(相信没有（bushi
}
