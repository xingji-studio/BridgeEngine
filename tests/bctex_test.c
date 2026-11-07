#include "BridgeEngine.h"
#include "internal/engine/texture_bc.h"
#include "internal/platform/platform.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct plat_renderer {
	int unused;
};

static struct plat_renderer fake_renderer;
static int					dummy_texture;
static uint8_t				last_pixels[4 * 4 * 4];
static int					update_count;
static int					destroy_count;

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

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

static plat_texture_t create_texture(plat_renderer_t renderer, plat_pixel_format_t format,
									 plat_texture_access_t access, int width, int height)
{
	(void)renderer;
	(void)access;
	(void)width;
	(void)height;
	if (format != PLAT_PIXELFORMAT_ARGB8888) return NULL;
	return (plat_texture_t)&dummy_texture;
}

static int update_texture(plat_texture_t texture, const void *pixels, int pitch)
{
	(void)texture;
	(void)pitch;
	memcpy(last_pixels, pixels, sizeof(last_pixels));
	update_count++;
	return 0;
}

static int get_texture_size(plat_texture_t texture, int *width, int *height)
{
	(void)texture;
	*width	= 4;
	*height = 4;
	return 0;
}

static void destroy_texture(plat_texture_t texture)
{
	(void)texture;
	destroy_count++;
}

static const plat_interface_t test_platform = {
	.texture = {.create_texture = create_texture,
				.update_texture = update_texture,
				.get_texture_size = get_texture_size,
				.destroy_texture = destroy_texture},
};

const plat_interface_t *plat_get(void)
{
	return &test_platform;
}

static void put_u32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t)(value & 0xFF);
	data[1] = (uint8_t)((value >> 8) & 0xFF);
	data[2] = (uint8_t)((value >> 16) & 0xFF);
	data[3] = (uint8_t)((value >> 24) & 0xFF);
}

static void put_bctex_header(uint8_t *data, int format, uint32_t payload_size)
{
	memcpy(data, "BCTX", 4);
	data[4] = 1;
	data[5] = (uint8_t)format;
	data[6] = 0;
	data[7] = 0;
	put_u32(data + 8, 4);
	put_u32(data + 12, 4);
	put_u32(data + 16, payload_size);
}

int main(void)
{
	int result = 1;

	/* Solid red BC1 block: c0 = red, c1 = blue, all indices zero. */
	uint8_t bc1[8] = {0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t rgba[4 * 4 * 4];
	result &= expect(bapi_bc_decode(bc1, sizeof(bc1), BAPI_BC_FORMAT_BC1, 4, 4, rgba) == 0,
					 "BC1 decode succeeds");
	result &= expect(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255,
					 "BC1 decodes red");

	uint8_t bc3[16] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					   0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00};
	result &= expect(bapi_bc_decode(bc3, sizeof(bc3), BAPI_BC_FORMAT_BC3, 4, 4, rgba) == 0,
					 "BC3 decode succeeds");
	result &= expect(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255,
					 "BC3 decodes red with alpha");
	result &= expect(bapi_bc_decode(bc1, sizeof(bc1), BAPI_BC_FORMAT_BC1, 8, 8, rgba) != 0,
					 "truncated BC1 payload is rejected");

	uint8_t bctex[20 + 8];
	put_bctex_header(bctex, BAPI_BC_FORMAT_BC1, 8);
	memcpy(bctex + 20, bc1, 8);
	update_count = 0;
	bapi_texture_t texture = bapi_texture_load_from_memory(bctex, sizeof(bctex));
	result &= expect(texture != NULL, "bctex loads through the CPU fallback");
	result &= expect(update_count == 1, "fallback uploads decoded RGBA once");
	result &= expect(last_pixels[0] == 255 && last_pixels[1] == 0 && last_pixels[2] == 0,
					 "fallback decoded pixels reach the texture backend");
	int width = 0, height = 0;
	bapi_texture_get_size(texture, &width, &height);
	result &= expect(width == 4 && height == 4, "bctex size is reported");
	bapi_texture_destroy(texture);
	result &= expect(destroy_count == 1, "bctex texture is destroyed");

	uint8_t bad[20];
	put_bctex_header(bad, 9, 0);
	result &= expect(bapi_texture_load_from_memory(bad, sizeof(bad)) == NULL,
					 "unknown bctex format is rejected");

	return result ? 0 : 1;
}
