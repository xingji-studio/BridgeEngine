#include "mouse_drawing.h"
#include "render/render.h"
#include <SDL3/SDL.h>

extern SDL_Renderer *renderer;

static bool isDrawing = false;
static int lastX, lastY;

void mouse_drawing_init(void) {
    isDrawing = false;
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
            engine_render_drawpixel((float)currentX, (float)currentY, 0xffffffff);
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
