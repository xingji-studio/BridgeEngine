#pragma once

#include "../bapi_types.h"
#include "../bapi.h"

#ifdef __cplusplus
extern "C" {
#endif

bapi_button_t* bapi_create_button(float x, float y, float w, float h, const char* text, bapi_color_t normal_color, bapi_color_t hover_color, bapi_color_t click_color, bapi_color_t text_color, float text_size);
void bapi_destroy_button(bapi_button_t* button);
int bapi_button_update(bapi_button_t* button, const bapi_event_t* event);
void bapi_button_render(bapi_button_t* button);

int bapi_button_is_clicked(bapi_button_t* button);
int bapi_button_is_hovered(bapi_button_t* button);

#ifdef __cplusplus
}
#endif


