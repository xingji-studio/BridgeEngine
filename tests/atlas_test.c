#include "BridgeEngine.h"
#include "internal/engine/texture_internal.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct plat_window {
	int unused;
};

struct plat_renderer {
	int unused;
};

static struct plat_renderer fake_renderer;
static int					dummy_texture;
static const char		   *g_manifest;
static int					render_calls;
static float				rec_src_x, rec_src_y, rec_src_w, rec_src_h;
static float				rec_dst_x, rec_dst_y, rec_dst_w, rec_dst_h;
static uint8_t				rec_r, rec_g, rec_b, rec_a;

plat_renderer_t bapi_internal_get_renderer(void)
{
	return &fake_renderer;
}

uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size)
{
	(void)path;
	if (g_manifest == NULL) return NULL;
	size_t	 length = strlen(g_manifest);
	uint8_t *copy	= malloc(length);
	if (copy == NULL) return NULL;
	memcpy(copy, g_manifest, length);
	if (out_size) *out_size = length;
	return copy;
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
	render_calls++;
}

static void render_texture_ex(plat_renderer_t renderer, plat_texture_t texture,
							  const plat_rectf_t *src, const plat_rectf_t *dst,
							  const plat_pointf_t *pivot, float rotation, int flip, uint8_t r,
							  uint8_t g, uint8_t b, uint8_t a)
{
	(void)renderer;
	(void)texture;
	(void)pivot;
	(void)rotation;
	(void)flip;
	render_calls++;
	if (src) {
		rec_src_x = src->x;
		rec_src_y = src->y;
		rec_src_w = src->w;
		rec_src_h = src->h;
	}
	rec_dst_x = dst->x;
	rec_dst_y = dst->y;
	rec_dst_w = dst->w;
	rec_dst_h = dst->h;
	rec_r	  = r;
	rec_g	  = g;
	rec_b	  = b;
	rec_a	  = a;
}

static const plat_interface_t test_platform = {
	.renderer = {.render_texture = render_texture, .render_texture_ex = render_texture_ex},
};

const plat_interface_t *plat_get(void)
{
	return &test_platform;
}

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

int main(void)
{
	int result = 1;

	struct bapi_texture_internal texture;
	memset(&texture, 0, sizeof(texture));
	texture.platform_texture = (plat_texture_t)&dummy_texture;
	texture.width			 = 64;
	texture.height			 = 32;

	g_manifest = "BE_ATLAS 1\n"
				 "page missing.png\n"
				 "size 64 32\n"
				 "region hero 0 0 16 16\n"
				 "region enemy 16 0 16 16\n";

	bapi_atlas_t atlas = bapi_atlas_load_with_texture("fixture.atlas", &texture);
	result &= expect(atlas != NULL, "atlas loads from a valid manifest");
	result &= expect(bapi_atlas_get_region_count(atlas) == 2, "region count");
	result &=
		expect(bapi_atlas_get_texture(atlas) == &texture, "borrowed page texture is returned");
	result &= expect(bapi_atlas_get_region(atlas, 0) != NULL &&
						 strcmp(bapi_atlas_get_region(atlas, 0)->name, "hero") == 0,
					 "region order");
	result &=
		expect(bapi_atlas_get_region(atlas, -1) == NULL && bapi_atlas_get_region(atlas, 2) == NULL,
			   "region bounds are checked");
	const bapi_atlas_region_t *enemy = bapi_atlas_find_region(atlas, "enemy");
	result &= expect(enemy != NULL && enemy->x == 16 && enemy->w == 16, "region lookup");
	result &=
		expect(bapi_atlas_find_region(atlas, "missing") == NULL, "unknown region returns NULL");

	render_calls = 0;
	bapi_atlas_draw(atlas, "hero", (bapi_rect_t){10.0f, 20.0f, 32.0f, 32.0f}, BAPI_FLIP_VERTICAL,
					(bapi_color_t){255, 128, 64, 200});
	result &= expect(render_calls == 1, "atlas draw issues one sprite draw");
	result &=
		expect(rec_src_x == 0.0f && rec_src_y == 0.0f && rec_src_w == 16.0f && rec_src_h == 16.0f,
			   "atlas source rectangle comes from the region");
	result &=
		expect(rec_dst_x == 10.0f && rec_dst_y == 20.0f && rec_dst_w == 32.0f && rec_dst_h == 32.0f,
			   "atlas destination rectangle");
	result &= expect(rec_r == 255 && rec_g == 128 && rec_b == 64 && rec_a == 200,
					 "atlas tint is forwarded");

	render_calls = 0;
	bapi_clear_last_error();
	bapi_atlas_draw(atlas, "missing", (bapi_rect_t){0, 0, 1, 1}, 0,
					(bapi_color_t){255, 255, 255, 255});
	result &= expect(render_calls == 0, "missing region does not draw");
	result &= expect(bapi_get_last_error()[0] != '\0', "missing region records an error");

	bapi_atlas_destroy(atlas);

	const char *memory_manifest = "BE_ATLAS 1\npage ignored.bmp\nregion one 0 0 8 8\n";
	bapi_atlas_t memory_atlas = bapi_atlas_load_from_memory(memory_manifest, strlen(memory_manifest),
												&texture);
	result &= expect(memory_atlas != NULL, "atlas loads from memory");
	result &= expect(bapi_atlas_get_region_count(memory_atlas) == 1, "memory atlas region count");
	result &= expect(bapi_atlas_find_region(memory_atlas, "one") != NULL, "memory atlas lookup");
	bapi_atlas_destroy(memory_atlas);
	result &= expect(bapi_atlas_load_from_memory(NULL, 0, &texture) == NULL,
					 "empty memory manifest is rejected");

	g_manifest = NULL;
	result &= expect(bapi_atlas_load_with_texture("fixture.atlas", &texture) == NULL,
					 "missing manifest returns NULL");
	result &= expect(bapi_atlas_load("fixture.atlas") == NULL, "unreadable page fails the load");

	g_manifest = "BE_ATLAS 1\nregion hero 0 0 16 16\n";
	result &= expect(bapi_atlas_load_with_texture("fixture.atlas", &texture) == NULL,
					 "manifest without a page is rejected");

	g_manifest = "not an atlas\n";
	result &= expect(bapi_atlas_load_with_texture("fixture.atlas", &texture) == NULL,
					 "unknown header is rejected");

	return result ? 0 : 1;
}
