#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Resets timing state. Called by the runtime on start and stop. */
void bapi_frame_reset(void);

#ifdef __cplusplus
}
#endif
