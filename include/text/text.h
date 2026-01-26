#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void bapi_draw_text(const char* text, float x, float y, float size, bapi_color_t color);
void bapi_get_text_size(const char* text, float size, float* width, float* height);

#ifdef __cplusplus
}
#endif
