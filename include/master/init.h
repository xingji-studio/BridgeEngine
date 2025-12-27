#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int bapi_engine_init(const char* title, int width, int height);
void bapi_engine_quit(void);
void bapi_engine_render_create(void);
bapi_window_t bapi_engine_get_window(void);
bapi_renderer_t bapi_engine_get_renderer(void);

#ifdef __cplusplus
}
#endif
