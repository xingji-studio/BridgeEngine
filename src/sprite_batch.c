#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/texture_internal.h"
#include "internal/engine/transform.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <string.h>

#define BAPI_BATCH_MAX_QUADS	1024
#define BAPI_BATCH_MAX_VERTICES (BAPI_BATCH_MAX_QUADS * 4)
#define BAPI_BATCH_MAX_INDICES	(BAPI_BATCH_MAX_QUADS * 6)

typedef struct {
	int			   active;
	int			   fallback;
	bapi_texture_t texture;
	int			   vertex_count;
	int			   index_count;
	plat_vertex_t  vertices[BAPI_BATCH_MAX_VERTICES];
	int			   indices[BAPI_BATCH_MAX_INDICES];
} bapi_sprite_batch_t;

static bapi_sprite_batch_t g_batch;

static void batch_flush(void)
{
	if (g_batch.vertex_count == 0) {
		g_batch.texture = NULL;
		return;
	}

	const plat_interface_t *plat = plat_get();
	if (!g_batch.fallback && plat != NULL && plat->renderer.render_geometry_textured != NULL &&
		g_batch.texture != NULL && g_batch.texture->platform_texture != NULL) {
		plat->renderer.render_geometry_textured(
			bapi_internal_get_renderer(), g_batch.texture->platform_texture, g_batch.vertices,
			g_batch.vertex_count, g_batch.indices, g_batch.index_count);
	}

	g_batch.vertex_count = 0;
	g_batch.index_count	 = 0;
	g_batch.texture		 = NULL;
}

void bapi_sprite_batch_begin(void)
{
	if (g_batch.active) batch_flush();

	const plat_interface_t *plat = plat_get();
	g_batch.active				 = 1;
	g_batch.fallback	 = plat == NULL || plat->renderer.render_geometry_textured == NULL ||
						   bapi_internal_get_renderer() == NULL;
	g_batch.texture		 = NULL;
	g_batch.vertex_count = 0;
	g_batch.index_count	 = 0;
}

void bapi_sprite_batch_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	if (texture == NULL || params == NULL) return;
	if (params->dst.w == 0.0f || params->dst.h == 0.0f) return;

	if (!g_batch.active || g_batch.fallback) {
		bapi_texture_draw(texture, params);
		return;
	}

	if (texture != g_batch.texture) batch_flush();
	if (g_batch.vertex_count + 4 > BAPI_BATCH_MAX_VERTICES ||
		g_batch.index_count + 6 > BAPI_BATCH_MAX_INDICES) {
		batch_flush();
	}
	g_batch.texture = texture;

	float texture_width	 = (float)texture->width;
	float texture_height = (float)texture->height;
	if (texture_width <= 0.0f || texture_height <= 0.0f) return;

	float src_x = params->src.x;
	float src_y = params->src.y;
	float src_w = params->src.w;
	float src_h = params->src.h;
	if (src_w <= 0.0f || src_h <= 0.0f) {
		src_x = 0.0f;
		src_y = 0.0f;
		src_w = texture_width;
		src_h = texture_height;
	}

	float u0 = src_x / texture_width;
	float v0 = src_y / texture_height;
	float u1 = (src_x + src_w) / texture_width;
	float v1 = (src_y + src_h) / texture_height;
	if (params->flip & BAPI_FLIP_HORIZONTAL) {
		float tmp = u0;
		u0		  = u1;
		u1		  = tmp;
	}
	if (params->flip & BAPI_FLIP_VERTICAL) {
		float tmp = v0;
		v0		  = v1;
		v1		  = tmp;
	}

	float pivot_x = params->dst.x + params->pivot_x * params->dst.w;
	float pivot_y = params->dst.y + params->pivot_y * params->dst.h;
	float c		  = cosf(params->rotation);
	float s		  = sinf(params->rotation);

	float local_x[4] = {params->dst.x, params->dst.x + params->dst.w, params->dst.x + params->dst.w,
						params->dst.x};
	float local_y[4] = {params->dst.y, params->dst.y, params->dst.y + params->dst.h,
						params->dst.y + params->dst.h};
	float u[4]		 = {u0, u1, u1, u0};
	float v[4]		 = {v0, v0, v1, v1};

	int base = g_batch.vertex_count;
	for (int i = 0; i < 4; i++) {
		float		dx	  = local_x[i] - pivot_x;
		float		dy	  = local_y[i] - pivot_y;
		float		rx	  = dx * c - dy * s + pivot_x;
		float		ry	  = dx * s + dy * c + pivot_y;
		bapi_vec2_t world = bapi_transform_point((bapi_vec2_t){rx, ry});

		plat_vertex_t *vertex = &g_batch.vertices[base + i];
		vertex->x			  = world.x;
		vertex->y			  = world.y;
		vertex->u			  = u[i];
		vertex->v			  = v[i];
		vertex->r			  = params->tint.r;
		vertex->g			  = params->tint.g;
		vertex->b			  = params->tint.b;
		vertex->a			  = params->tint.a;
	}

	g_batch.indices[g_batch.index_count++] = base + 0;
	g_batch.indices[g_batch.index_count++] = base + 1;
	g_batch.indices[g_batch.index_count++] = base + 2;
	g_batch.indices[g_batch.index_count++] = base + 0;
	g_batch.indices[g_batch.index_count++] = base + 2;
	g_batch.indices[g_batch.index_count++] = base + 3;
	g_batch.vertex_count += 4;
}

void bapi_sprite_batch_end(void)
{
	if (!g_batch.active) return;
	batch_flush();
	g_batch.active = 0;
}
