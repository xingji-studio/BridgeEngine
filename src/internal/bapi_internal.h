#pragma once

#include "internal/platform/platform_types.h"
#include <stdint.h>

#include "BridgeEngine.h"

#ifdef __cplusplus
extern "C" {
#endif

plat_renderer_t bapi_internal_get_renderer(void);
uint32_t		bapi_get_ticks(void);
void			bapi_set_last_error(const char *format, ...);
plat_audio_device_t bapi_audio_device_handle(void);
float			bapi_audio_master_volume(void);
void			bapi_music_cleanup(void);
void			bapi_music_apply_master_volume(float master);
void		   *bapi_internal_get_native_window(void);
void		   *bapi_internal_get_native_renderer(void);

#ifdef __cplusplus
}
#endif
