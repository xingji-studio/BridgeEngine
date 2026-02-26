#pragma once

#include "bapi_types.h"
#include "bapi_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

int bapi_engine_init(const char* title, int width, int height);
void bapi_engine_quit(void);
bapi_window_t bapi_engine_get_window(void);
bapi_renderer_t bapi_engine_get_renderer(void);

int bapi_poll_event(bapi_event_t* event);
int bapi_event_get_type(const bapi_event_t* event);
uint8_t bapi_event_get_key_code(const bapi_event_t* event);
int bapi_event_get_mouse_x(const bapi_event_t* event);
int bapi_event_get_mouse_y(const bapi_event_t* event);
int bapi_event_get_mouse_button(const bapi_event_t* event);
int bapi_event_get_motion_x(const bapi_event_t* event);
int bapi_event_get_motion_y(const bapi_event_t* event);
int bapi_event_is_mouse_button_down(const bapi_event_t* event);
int bapi_event_is_mouse_button_up(const bapi_event_t* event);
int bapi_event_is_mouse_motion(const bapi_event_t* event);

void bapi_render_clear(void);
void bapi_render_present(void);
void bapi_set_render_color(bapi_color_t color);
void bapi_delay(uint32_t ms);

void bapi_draw_pixel(float x, float y, bapi_color_t color);
void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);

void bapi_draw_image(const char* filepath, float x, float y, float w, float h);

void bapi_draw_text(const char* text, float x, float y, float size, bapi_color_t color);
void bapi_get_text_size(const char* text, float size, float* width, float* height);
void bapi_text_init(void);
void bapi_text_cleanup(void);

void bapi_mouse_init(void);
void bapi_mouse_handle_event(const bapi_event_t* event);
void bapi_mouse_render(void);
void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_mouse_clear(void);
void bapi_mouse_cleanup(void);
#include "button/button.h"
#include "scene/scene.h"
#include "level/level.h"
#include "xml/xml_loader.h"

bapi_color_t bapi_color_from_hex(uint32_t hex_color);
bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

uint32_t bapi_get_ticks(void);

#ifdef __cplusplus
}
#endif
