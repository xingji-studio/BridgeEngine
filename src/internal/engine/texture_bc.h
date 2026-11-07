#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAPI_BC_FORMAT_BC1 1
#define BAPI_BC_FORMAT_BC3 3

/* Decode a BC1/BC3 payload into tightly packed RGBA8888 (width * height * 4). */
int bapi_bc_decode(const void *data, size_t size, int format, int width, int height, uint8_t *rgba);

#ifdef __cplusplus
}
#endif
