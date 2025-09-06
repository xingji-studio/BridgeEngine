#include "mouse_drawing.h"
#include "render/render.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>

extern SDL_Renderer *renderer;

#define MAX_LINES 1000

typedef struct {
    float x1, y1, x2, y2;
    int color;
} Line;

static bool isDrawing = false;
static int lastX, lastY;
static Line lines[MAX_LINES];
static int lineCount = 0;

void bapi_mouse_drawing_init(void) {
    isDrawing = false;
    lineCount = 0;
}

void bapi_mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color) {
    float dx = fabsf(x2 - x1);
    float dy = fabsf(y2 - y1);
    float sx = (x1 < x2) ? 1.0f : -1.0f;
    float sy = (y1 < y2) ? 1.0f : -1.0f;
    float err = dx - dy;

    while (1) {
        bapi_engine_render_drawpixel(x1, y1, color);
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

void bapi_mouse_drawing_handle_event(SDL_Event *event) {
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
            
            // 保存线条到数组
            if (lineCount < MAX_LINES) {
                lines[lineCount].x1 = (float)lastX;
                lines[lineCount].y1 = (float)lastY;
                lines[lineCount].x2 = (float)currentX;
                lines[lineCount].y2 = (float)currentY;
                lines[lineCount].color = 0xffffffff;
                lineCount++;
            }
            
            bapi_mouse_drawing_draw_line((float)lastX, (float)lastY, 
                                  (float)currentX, (float)currentY, 0xffffffff);
            lastX = currentX;
            lastY = currentY;
        }
    }
}

void bapi_mouse_drawing_render(void) {
    // 重新绘制所有保存的线条
    for (int i = 0; i < lineCount; i++) {
        bapi_mouse_drawing_draw_line(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2, lines[i].color);
    }
}

void bapi_mouse_drawing_cleanup(void) {
    // 这里可以添加清理资源的逻辑，如果有的话:)(相信没有（bushi
}

void bapi_mouse_drawing_clear_lines(void) {
    lineCount = 0;
}