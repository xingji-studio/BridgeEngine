#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int bapi_text_init(const char* font_path, int font_size);
bapi_texture_t bapi_render_text(const char* text, bapi_color_t color);
void bapi_draw_text(bapi_texture_t text_texture, float x, float y, float w, float h);
void bapi_destroy_text(bapi_texture_t text_texture);
void bapi_text_cleanup(void);

#ifdef __cplusplus
}
#endif
