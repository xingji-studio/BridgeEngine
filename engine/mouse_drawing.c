#include "mouse_drawing.h"
#include "master/init.h"
#include "bapi.h"
#include "bapi_internal.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>

#define MAX_LINES 1000

typedef struct {
    float x1, y1, x2, y2;
    bapi_color_t color;
} Line;

static bool isDrawing = false;
static int lastX, lastY;
static Line lines[MAX_LINES];
static int lineCount = 0;

void bapi_mouse_init(void) {
    isDrawing = false;
    lineCount = 0;
}

void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color) {
    bapi_draw_line(x1, y1, x2, y2, color);
}

void bapi_mouse_handle_event(const bapi_event_t* event) {
    if (bapi_event_is_mouse_button_down(event)) {
        if (bapi_event_get_mouse_button(event) == BAPI_BUTTON_LEFT) {
            isDrawing = true;
            lastX = bapi_event_get_mouse_x(event);
            lastY = bapi_event_get_mouse_y(event);
        }
    } else if (bapi_event_is_mouse_button_up(event)) {
        if (bapi_event_get_mouse_button(event) == BAPI_BUTTON_LEFT) {
            isDrawing = false;
        }
    } else if (bapi_event_is_mouse_motion(event)) {
        if (isDrawing) {
            int currentX = bapi_event_get_motion_x(event);
            int currentY = bapi_event_get_motion_y(event);

            if (lineCount < MAX_LINES) {
                lines[lineCount].x1 = (float)lastX;
                lines[lineCount].y1 = (float)lastY;
                lines[lineCount].x2 = (float)currentX;
                lines[lineCount].y2 = (float)currentY;
                lines[lineCount].color = bapi_color(255, 255, 255, 255);
                lineCount++;
            }

            bapi_mouse_draw_line((float)lastX, (float)lastY,
                                  (float)currentX, (float)currentY, bapi_color(255, 255, 255, 255));
            lastX = currentX;
            lastY = currentY;
        }
    }
}

void bapi_mouse_render(void) {
    for (int i = 0; i < lineCount; i++) {
        bapi_mouse_draw_line(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2, lines[i].color);
    }
}

void bapi_mouse_cleanup(void) {
    isDrawing = false;
    lineCount = 0;
}

void bapi_mouse_clear(void) {
    lineCount = 0;
}

void bapi_mouse_drawing_init(void) {
    bapi_mouse_init();
}

void bapi_mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color) {
    bapi_color_t c = bapi_color_from_hex((uint32_t)color);
    bapi_mouse_draw_line(x1, y1, x2, y2, c);
}

void bapi_mouse_drawing_clear_lines(void) {
    bapi_mouse_clear();
}
