#pragma once

#include "BridgeEngine.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	float a;
	float b;
	float c;
	float d;
	float e;
	float f;
} bapi_transform_t;

bool			 bapi_transform_is_identity(void);
bapi_transform_t bapi_transform_current(void);
void			 bapi_transform_push_matrix(const bapi_transform_t *matrix);
void  bapi_transform_push_matrix_rotation(const bapi_transform_t *matrix, float rotation);
float bapi_transform_rotation(void);
float bapi_transform_scale_x(void);
float bapi_transform_scale_y(void);

#ifdef __cplusplus
}
#endif
