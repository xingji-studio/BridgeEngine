#include "internal/engine/transform.h"
#include "BridgeEngine.h"

#include <math.h>
#include <string.h>

#define BAPI_TRANSFORM_MAX_DEPTH 32

typedef struct {
	bapi_transform_t matrix;
	float			 rotation;
} bapi_transform_entry_t;

typedef struct {
	bapi_transform_entry_t stack[BAPI_TRANSFORM_MAX_DEPTH];
	int					   depth;
} bapi_transform_state_t;

static bapi_transform_state_t g_transform;

static bapi_transform_t transform_identity(void)
{
	bapi_transform_t m = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	return m;
}

static bapi_transform_t transform_multiply(const bapi_transform_t *outer,
										   const bapi_transform_t *inner)
{
	/* outer(inner(x)) */
	bapi_transform_t result;
	result.a = outer->a * inner->a + outer->c * inner->b;
	result.b = outer->b * inner->a + outer->d * inner->b;
	result.c = outer->a * inner->c + outer->c * inner->d;
	result.d = outer->b * inner->c + outer->d * inner->d;
	result.e = outer->a * inner->e + outer->c * inner->f + outer->e;
	result.f = outer->b * inner->e + outer->d * inner->f + outer->f;
	return result;
}

static bapi_transform_entry_t *transform_top(void)
{
	if (g_transform.depth <= 0) return NULL;
	return &g_transform.stack[g_transform.depth - 1];
}

void bapi_reset_transform(void)
{
	memset(&g_transform, 0, sizeof(g_transform));
}

void bapi_transform_push_matrix_rotation(const bapi_transform_t *matrix, float rotation)
{
	if (matrix == NULL) return;
	if (g_transform.depth <= 0) {
		g_transform.stack[0].matrix	  = *matrix;
		g_transform.stack[0].rotation = rotation;
		g_transform.depth			  = 1;
		return;
	}
	if (g_transform.depth >= BAPI_TRANSFORM_MAX_DEPTH) return;
	g_transform.stack[g_transform.depth].matrix =
		transform_multiply(&g_transform.stack[g_transform.depth - 1].matrix, matrix);
	g_transform.stack[g_transform.depth].rotation =
		g_transform.stack[g_transform.depth - 1].rotation + rotation;
	g_transform.depth++;
}

void bapi_transform_push_matrix(const bapi_transform_t *matrix)
{
	bapi_transform_push_matrix_rotation(matrix, 0.0f);
}

void bapi_push_transform(bapi_vec2_t translation, float rotation, bapi_vec2_t scale)
{
	bapi_transform_t local;
	float			 c = cosf(rotation);
	float			 s = sinf(rotation);

	local.a = c * scale.x;
	local.b = s * scale.x;
	local.c = -s * scale.y;
	local.d = c * scale.y;
	local.e = translation.x;
	local.f = translation.y;

	bapi_transform_push_matrix_rotation(&local, rotation);
}

void bapi_pop_transform(void)
{
	if (g_transform.depth > 0) g_transform.depth--;
}

void bapi_apply_camera(const bapi_camera_t *camera)
{
	if (camera == NULL) return;
	float zoom = camera->zoom > 0.0f ? camera->zoom : 1.0f;
	float c	   = cosf(camera->rotation);
	float s	   = sinf(camera->rotation);

	bapi_transform_t m;
	m.a = zoom * c;
	m.b = -zoom * s;
	m.c = zoom * s;
	m.d = zoom * c;
	m.e = zoom * (-camera->x * c - camera->y * s) + camera->viewport_w * 0.5f;
	m.f = zoom * (camera->x * s - camera->y * c) + camera->viewport_h * 0.5f;
	bapi_transform_push_matrix_rotation(&m, -camera->rotation);
}

bapi_vec2_t bapi_transform_point(bapi_vec2_t point)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return point;
	bapi_vec2_t result;
	result.x = top->matrix.a * point.x + top->matrix.c * point.y + top->matrix.e;
	result.y = top->matrix.b * point.x + top->matrix.d * point.y + top->matrix.f;
	return result;
}

bapi_vec2_t bapi_transform_vector(bapi_vec2_t vector)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return vector;
	bapi_vec2_t result;
	result.x = top->matrix.a * vector.x + top->matrix.c * vector.y;
	result.y = top->matrix.b * vector.x + top->matrix.d * vector.y;
	return result;
}

float bapi_transform_scale_factor(void)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return 1.0f;
	float determinant = top->matrix.a * top->matrix.d - top->matrix.b * top->matrix.c;
	if (determinant < 0.0f) determinant = -determinant;
	float factor = sqrtf(determinant);
	return factor > 0.0f ? factor : 1.0f;
}

float bapi_transform_rotation(void)
{
	bapi_transform_entry_t *top = transform_top();
	return top == NULL ? 0.0f : top->rotation;
}

float bapi_transform_scale_x(void)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return 1.0f;
	float length = sqrtf(top->matrix.a * top->matrix.a + top->matrix.b * top->matrix.b);
	return length > 0.0f ? length : 1.0f;
}

float bapi_transform_scale_y(void)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return 1.0f;
	float length = sqrtf(top->matrix.c * top->matrix.c + top->matrix.d * top->matrix.d);
	return length > 0.0f ? length : 1.0f;
}

bool bapi_transform_is_identity(void)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return true;
	return top->matrix.a == 1.0f && top->matrix.b == 0.0f && top->matrix.c == 0.0f &&
		   top->matrix.d == 1.0f && top->matrix.e == 0.0f && top->matrix.f == 0.0f;
}

bapi_transform_t bapi_transform_current(void)
{
	bapi_transform_entry_t *top = transform_top();
	if (top == NULL) return transform_identity();
	return top->matrix;
}
