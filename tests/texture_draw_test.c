#include "BridgeEngine.h"
#include "internal/engine/texture_internal.h"
#include "internal/engine/transform.h"
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
static int					  dummy_texture;
static int					  use_basic_platform;
static const plat_interface_t ex_platform;
static const plat_interface_t basic_platform;

static int	   texture_calls;
static int	   texture_ex_calls;
static float   rec_src_x, rec_src_y, rec_src_w, rec_src_h;
static int	   rec_has_src;
static float   rec_dst_x, rec_dst_y, rec_dst_w, rec_dst_h;
static float   rec_pivot_x, rec_pivot_y;
static float   rec_rotation;
static int	   rec_flip;
static uint8_t rec_r, rec_g, rec_b, rec_a;
static float   rec_rect_x, rec_rect_y, rec_rect_w, rec_rect_h;

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
	texture_calls++;
	rec_rect_x = x;
	rec_rect_y = y;
	rec_rect_w = w;
	rec_rect_h = h;
}

static void render_texture_ex(plat_renderer_t renderer, plat_texture_t texture,
							  const plat_rectf_t *src, const plat_rectf_t *dst,
							  const plat_pointf_t *pivot, float rotation, int flip, uint8_t r,
							  uint8_t g, uint8_t b, uint8_t a)
{
	(void)renderer;
	(void)texture;
	texture_ex_calls++;
	rec_has_src = src != NULL;
	if (src) {
		rec_src_x = src->x;
		rec_src_y = src->y;
		rec_src_w = src->w;
		rec_src_h = src->h;
	}
	rec_dst_x	 = dst->x;
	rec_dst_y	 = dst->y;
	rec_dst_w	 = dst->w;
	rec_dst_h	 = dst->h;
	rec_pivot_x	 = pivot ? pivot->x : -1.0f;
	rec_pivot_y	 = pivot ? pivot->y : -1.0f;
	rec_rotation = rotation;
	rec_flip	 = flip;
	rec_r		 = r;
	rec_g		 = g;
	rec_b		 = b;
	rec_a		 = a;
}

static const plat_interface_t ex_platform = {
	.renderer = {.render_texture = render_texture, .render_texture_ex = render_texture_ex},
};

static const plat_interface_t basic_platform = {
	.renderer = {.render_texture = render_texture},
};

int main(void)
{
	int							 result = 1;
	struct bapi_texture_internal texture;
	memset(&texture, 0, sizeof(texture));
	texture.platform_texture = (plat_texture_t)&dummy_texture;
	texture.width			 = 64;
	texture.height			 = 32;

	bapi_texture_draw_t params =
		bapi_texture_draw_default((bapi_rect_t){10.0f, 20.0f, 100.0f, 50.0f});
	result &= expect(params.pivot_x == 0.5f && params.pivot_y == 0.5f, "default pivot is centered");
	result &= expect(params.tint.r == 255 && params.tint.a == 255, "default tint is opaque white");
	result &=
		expect(params.src.w == 0.0f && params.src.h == 0.0f, "default source is the whole texture");

	bapi_texture_draw(&texture, &params);
	result &= expect(texture_ex_calls == 1, "sprite path uses render_texture_ex");
	result &= expect(!rec_has_src, "empty source rectangle means full texture");
	result &= expect(rec_dst_x == 10.0f && rec_dst_y == 20.0f && rec_dst_w == 100.0f &&
						 rec_dst_h == 50.0f,
					 "identity destination rectangle is preserved");
	result &= expect(rec_pivot_x == 50.0f && rec_pivot_y == 25.0f,
					 "pivot is expressed in destination pixels");

	bapi_push_transform((bapi_vec2_t){10.0f, 10.0f}, 0.0f, (bapi_vec2_t){2.0f, 2.0f});
	bapi_texture_draw(&texture, &params);
	result &= expect(rec_dst_x == 30.0f && rec_dst_y == 50.0f && rec_dst_w == 200.0f &&
						 rec_dst_h == 100.0f,
					 "translation and scale are applied to the sprite");
	result &=
		expect(rec_pivot_x == 100.0f && rec_pivot_y == 50.0f, "pivot scales with the destination");
	bapi_pop_transform();

	bapi_push_transform((bapi_vec2_t){0.0f, 0.0f}, 1.57079632679f, (bapi_vec2_t){1.0f, 1.0f});
	bapi_texture_draw(&texture, &params);
	result &= expect(fabsf(rec_rotation - 1.5707963f) < 0.0001f, "rotation reaches the backend");
	bapi_pop_transform();

	params.src	= (bapi_rect_t){8.0f, 4.0f, 16.0f, 16.0f};
	params.flip = BAPI_FLIP_HORIZONTAL;
	params.tint = (bapi_color_t){128, 64, 32, 200};
	bapi_texture_draw(&texture, &params);
	result &= expect(rec_has_src && rec_src_x == 8.0f && rec_src_y == 4.0f && rec_src_w == 16.0f &&
						 rec_src_h == 16.0f,
					 "source rectangle is forwarded");
	result &= expect(rec_flip == BAPI_FLIP_HORIZONTAL, "flip flags are forwarded");
	result &=
		expect(rec_r == 128 && rec_g == 64 && rec_b == 32 && rec_a == 200, "tint is forwarded");

	bapi_clear_last_error();
	bapi_texture_t missing = bapi_texture_from_file("missing.png", NULL, NULL);
	result &= expect(missing == NULL, "missing texture returns NULL");
	result &= expect(bapi_get_last_error()[0] != '\0', "texture failure records a last error");
	bapi_clear_last_error();
	result &= expect(bapi_get_last_error()[0] == '\0', "clear_last_error resets the message");

	/* Fallback path: no render_texture_ex slot. */
	use_basic_platform = 1;
	texture_calls	   = 0;
	bapi_texture_render_ex(&texture, 10.0f, 20.0f, 100.0f, 50.0f);
	result &= expect(texture_calls == 1 && rec_rect_x == 10.0f && rec_rect_y == 20.0f,
					 "identity texture_render_ex uses the simple platform path");

	bapi_reset_transform();
	bapi_push_transform((bapi_vec2_t){5.0f, 5.0f}, 0.0f, (bapi_vec2_t){2.0f, 2.0f});
	params = bapi_texture_draw_default((bapi_rect_t){0.0f, 0.0f, 10.0f, 10.0f});
	bapi_texture_draw(&texture, &params);
	result &= expect(texture_calls == 2, "fallback path draws a transformed AABB");
	result &= expect(rec_rect_x == 5.0f && rec_rect_y == 5.0f && rec_rect_w == 20.0f &&
						 rec_rect_h == 20.0f,
					 "fallback AABB covers the transformed quad");
	bapi_pop_transform();

	return result ? 0 : 1;
}
