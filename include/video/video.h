#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_video_internal* bapi_video_t;

int bapi_video_init(void);
void bapi_video_cleanup(void);
bapi_video_t bapi_video_load(const char* filepath);
void bapi_video_free(bapi_video_t video);
int bapi_video_play(bapi_video_t video);
void bapi_video_pause(bapi_video_t video);
void bapi_video_stop(bapi_video_t video);
void bapi_video_render(bapi_video_t video, int x, int y, int w, int h);
void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h);
void bapi_video_render_center(bapi_video_t video, int window_w, int window_h);
void bapi_video_set_loop(bapi_video_t video, int loop);
void bapi_video_set_volume(bapi_video_t video, float volume);
int bapi_video_is_playing(bapi_video_t video);
void bapi_video_get_size(bapi_video_t video, int* w, int* h);
void bapi_video_update(void);

#ifdef __cplusplus
}
#endif
