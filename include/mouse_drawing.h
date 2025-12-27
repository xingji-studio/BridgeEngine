#pragma once

#include "bapi_types.h"
#include "bapi_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void bapi_mouse_init(void);
void bapi_mouse_handle_event(const bapi_event_t* event);
void bapi_mouse_render(void);
void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_mouse_clear(void);
void bapi_mouse_cleanup(void);

#ifdef __cplusplus
}
#endif
