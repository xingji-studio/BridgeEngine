#pragma once

#include "bapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_sound_internal* bapi_sound_t;

int bapi_audio_init(void);
void bapi_audio_cleanup(void);
bapi_sound_t bapi_sound_load(const char* filepath);
int bapi_sound_play(bapi_sound_t sound);
void bapi_sound_set_volume(bapi_sound_t sound, float volume);
void bapi_sound_set_loop(bapi_sound_t sound, int loop);
void bapi_sound_stop(bapi_sound_t sound);
void bapi_sound_update(void);
void bapi_sound_free(bapi_sound_t sound);

#ifdef __cplusplus
}
#endif
