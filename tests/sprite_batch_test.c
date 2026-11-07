#include "BridgeEngine.h"
#include "internal/engine/texture_internal.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct plat_window {
	int unused;
};

struct plat_renderer {
	int unused;
};

static struct plat_renderer	  fake_renderer;
static int					  dummy_texture_a;
static int					  dummy_texture_b;
static int					  use_basic_platform;
static const plat_interface_t ex_platform;
static const plat_interface_t basic_platform;

static int			 geometry_calls;
static int			 geometry_vertex_count;
static int			 geometry_index_count;
static int			 texture_calls;
static plat_vertex_t first_vertex;
static plat_vertex_t second_vertex;

plat_renderer_t bapi_internal_get_renderer(void)
{
	return &fake_renderer;
}

uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size)
{
	(void)path;
	(void)out_size;
	return NULL;
}

const plat_interface_t *plat_get(void)
{
	return use_basic_platform ? &basic_platform : &ex_platform;
}

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

static void render_texture(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
						   float w, float h)
{
	(void)renderer;
	(void)texture;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	texture_calls++;
}

static void render_geometry_textured(plat_renderer_t renderer, plat_texture_t texture,
									 const plat_vertex_t *vertices, int vertex_count,
									 const int *indices, int index_count)
{
	(void)renderer;
	(void)texture;
	(void)indices;
	geometry_calls++;
	geometry_vertex_count = vertex_count;
	geometry_index_count  = index_count;
	if (vertex_count > 0) first_vertex = vertices[0];
	if (vertex_count > 1) second_vertex = vertices[1];
}

static const plat_interface_t ex_platform = {
	.renderer = {.render_texture		   = render_texture,
				 .render_texture_ex		   = NULL,
				 .render_geometry_textured = render_geometry_textured},
};

static const plat_interface_t basic_platform = {
	.renderer = {.render_texture = render_texture},
};

int main(void)
{
	int							 result = 1;
	struct bapi_texture_internal texture_a;
	struct bapi_texture_internal texture_b;
	memset(&texture_a, 0, sizeof(texture_a));
	memset(&texture_b, 0, sizeof(texture_b));
	texture_a.platform_texture = (plat_texture_t)&dummy_texture_a;
	texture_a.width			   = 64;
	texture_a.height		   = 32;
	texture_b.platform_texture = (plat_texture_t)&dummy_texture_b;
	texture_b.width			   = 64;
	texture_b.height		   = 32;

	bapi_texture_draw_t params = bapi_texture_draw_default((bapi_rect_t){0.0f, 0.0f, 16.0f, 16.0f});
	params.src				   = (bapi_rect_t){0.0f, 0.0f, 16.0f, 16.0f};
	params.pivot_x			   = 0.0f;
	params.pivot_y			   = 0.0f;

	bapi_sprite_batch_begin();
	bapi_sprite_batch_draw(&texture_a, &params);
	params.dst.x = 16.0f;
	bapi_sprite_batch_draw(&texture_a, &params);
	result &= expect(geometry_calls == 0, "batch defers geometry until flush");
	bapi_sprite_batch_end();

	result &= expect(geometry_calls == 1, "batch flushes once for one texture");
	result &= expect(geometry_vertex_count == 8, "two sprites produce eight vertices");
	result &= expect(geometry_index_count == 12, "two sprites produce twelve indices");
	result &= expect(first_vertex.x == 0.0f && first_vertex.y == 0.0f, "first vertex position");
	result &= expect(second_vertex.x == 16.0f && second_vertex.y == 0.0f, "second vertex position");
	result &= expect(first_vertex.u == 0.0f && first_vertex.v == 0.0f, "top-left UV");
	result &= expect(second_vertex.u == 0.25f && second_vertex.v == 0.0f, "top-right UV");

	bapi_reset_transform();
	bapi_push_transform((bapi_vec2_t){10.0f, 20.0f}, 0.0f, (bapi_vec2_t){2.0f, 2.0f});
	bapi_sprite_batch_begin();
	params.dst = (bapi_rect_t){0.0f, 0.0f, 16.0f, 16.0f};
	bapi_sprite_batch_draw(&texture_a, &params);
	bapi_sprite_batch_end();
	result &= expect(first_vertex.x == 10.0f && first_vertex.y == 20.0f,
					 "batch applies the transform stack");
	result &=
		expect(second_vertex.x == 42.0f && second_vertex.y == 20.0f, "batch scales sprite width");
	bapi_pop_transform();

	geometry_calls = 0;
	bapi_sprite_batch_begin();
	bapi_sprite_batch_draw(&texture_a, &params);
	bapi_sprite_batch_draw(&texture_b, &params);
	bapi_sprite_batch_end();
	result &= expect(geometry_calls == 2, "texture change flushes the previous batch");

	params.flip = BAPI_FLIP_HORIZONTAL;
	params.tint = (bapi_color_t){128, 64, 32, 200};
	bapi_sprite_batch_begin();
	bapi_sprite_batch_draw(&texture_a, &params);
	bapi_sprite_batch_end();
	result &= expect(first_vertex.u == 0.25f, "horizontal flip swaps U coordinates");
	result &= expect(first_vertex.r == 128 && first_vertex.g == 64 && first_vertex.b == 32 &&
						 first_vertex.a == 200,
					 "tint is written per vertex");

	use_basic_platform = 1;
	texture_calls	   = 0;
	geometry_calls	   = 0;
	bapi_sprite_batch_begin();
	bapi_sprite_batch_draw(&texture_a, &params);
	bapi_sprite_batch_end();
	result &= expect(texture_calls == 1, "fallback draws immediately");
	result &= expect(geometry_calls == 0, "fallback does not call geometry");

	texture_calls = 0;
	bapi_sprite_batch_draw(&texture_a, &params);
	result &= expect(texture_calls == 1, "draw outside a batch is immediate");

	return result ? 0 : 1;
}
