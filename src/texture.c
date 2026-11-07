#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/texture_bc.h"
#include "internal/engine/texture_internal.h"
#include "internal/engine/transform.h"
#include "internal/platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bapi_texture_t g_texture_cache[BAPI_MAX_CACHED_TEXTURES];
static bapi_texture_t g_allocated_textures;
static int			  g_cache_evict_next;

static char *bapi_texture_copy_key(const char *filepath)
{
	size_t length = strlen(filepath) + 1;
	char  *copy	  = malloc(length);
	if (copy) memcpy(copy, filepath, length);
	return copy;
}

static void bapi_texture_remove_from_cache(bapi_texture_t texture)
{
	for (int index = 0; index < BAPI_MAX_CACHED_TEXTURES; index++) {
		if (g_texture_cache[index] == texture) {
			g_texture_cache[index] = NULL;
			return;
		}
	}
}

static void bapi_texture_remove_from_allocated(bapi_texture_t texture)
{
	bapi_texture_t *current = &g_allocated_textures;
	while (*current != NULL) {
		if (*current == texture) {
			*current = texture->next_allocated;
			return;
		}
		current = &(*current)->next_allocated;
	}
}

static void bapi_texture_free(bapi_texture_t texture)
{
	const plat_interface_t *plat = plat_get();
	bapi_texture_remove_from_cache(texture);
	bapi_texture_remove_from_allocated(texture);
	if (plat != NULL && texture->platform_texture != NULL) {
		plat->texture.destroy_texture(texture->platform_texture);
	}
	free(texture->cache_key);
	free(texture);
}

static void bapi_texture_release(bapi_texture_t texture)
{
	if (!texture || --texture->reference_count > 0) return;
	bapi_texture_free(texture);
}

static bapi_texture_t texture_from_surface(const char *label, plat_surface_t *surface)
{
	const plat_interface_t *plat = plat_get();

	plat_texture_t plat_tex =
		plat->texture.create_texture_from_surface(bapi_internal_get_renderer(), surface);
	plat->texture.destroy_surface(surface);

	if (!plat_tex) {
		printf("[TEXTURE] Failed to create texture from: %s\n", label);
		bapi_set_last_error("texture upload failed: %s", label);
		return NULL;
	}

	bapi_texture_t tex = calloc(1, sizeof(struct bapi_texture_internal));
	if (!tex) {
		plat->texture.destroy_texture(plat_tex);
		return NULL;
	}
	tex->platform_texture = plat_tex;
	tex->reference_count  = 1;
	if (plat->texture.get_texture_size(plat_tex, &tex->width, &tex->height) != 0 ||
		tex->width <= 0 || tex->height <= 0) {
		plat->texture.destroy_texture(plat_tex);
		free(tex);
		return NULL;
	}
	tex->next_allocated	 = g_allocated_textures;
	g_allocated_textures = tex;
	return tex;
}

static bapi_texture_t texture_from_pixels(const char *label, const uint8_t *rgba, int width,
										  int height)
{
	const plat_interface_t *plat = plat_get();
	if (plat == NULL || plat->texture.create_texture == NULL ||
		plat->texture.update_texture == NULL) {
		bapi_set_last_error("platform cannot create decoded textures");
		return NULL;
	}

	plat_texture_t plat_tex = plat->texture.create_texture(
		bapi_internal_get_renderer(), PLAT_PIXELFORMAT_ARGB8888, PLAT_TEXTUREACCESS_STREAMING, width,
		height);
	if (plat_tex == NULL || plat->texture.update_texture(plat_tex, rgba, width * 4) != 0) {
		if (plat_tex != NULL) plat->texture.destroy_texture(plat_tex);
		bapi_set_last_error("texture upload failed: %s", label);
		return NULL;
	}

	bapi_texture_t tex = calloc(1, sizeof(struct bapi_texture_internal));
	if (tex == NULL) {
		plat->texture.destroy_texture(plat_tex);
		return NULL;
	}
	tex->platform_texture = plat_tex;
	tex->reference_count  = 1;
	tex->width			  = width;
	tex->height			  = height;
	tex->next_allocated	  = g_allocated_textures;
	g_allocated_textures  = tex;
	return tex;
}

static int texture_is_bctex(const uint8_t *data, size_t size)
{
	return data != NULL && size >= 20 && memcmp(data, "BCTX", 4) == 0;
}

static uint32_t texture_read_u32(const uint8_t *data)
{
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
		   ((uint32_t)data[3] << 24);
}

static bapi_texture_t texture_from_bctex(const uint8_t *data, size_t size, const char *label)
{
	if (!texture_is_bctex(data, size)) return NULL;
	int format = data[5];
	int width  = (int)texture_read_u32(data + 8);
	int height = (int)texture_read_u32(data + 12);
	uint32_t payload_size = texture_read_u32(data + 16);
	if (format != BAPI_BC_FORMAT_BC1 && format != BAPI_BC_FORMAT_BC3) {
		bapi_set_last_error("unsupported .bctex format: %d", format);
		return NULL;
	}
	if (width <= 0 || height <= 0 || payload_size > size - 20) {
		bapi_set_last_error("invalid .bctex header: %s", label);
		return NULL;
	}
	int	   block_size = format == BAPI_BC_FORMAT_BC1 ? 8 : 16;
	size_t needed	  = (size_t)((width + 3) / 4) * (size_t)((height + 3) / 4) *
					  (size_t)block_size;
	if (payload_size < needed) {
		bapi_set_last_error("truncated .bctex payload: %s", label);
		return NULL;
	}
	const uint8_t *payload = data + 20;

	const plat_interface_t *plat = plat_get();
	if (plat != NULL && plat->texture.create_texture != NULL &&
		plat->texture.update_texture != NULL) {
		plat_pixel_format_t compressed_format =
			format == BAPI_BC_FORMAT_BC1 ? PLAT_PIXELFORMAT_BC1 : PLAT_PIXELFORMAT_BC3;
		plat_texture_t plat_tex = plat->texture.create_texture(
			bapi_internal_get_renderer(), compressed_format, PLAT_TEXTUREACCESS_STREAMING, width,
			height);
		if (plat_tex != NULL) {
			if (plat->texture.update_texture(plat_tex, payload, 0) == 0) {
				bapi_texture_t tex = calloc(1, sizeof(struct bapi_texture_internal));
				if (tex != NULL) {
					tex->platform_texture = plat_tex;
					tex->reference_count  = 1;
					tex->width			  = width;
					tex->height			  = height;
					tex->next_allocated	  = g_allocated_textures;
					g_allocated_textures  = tex;
					return tex;
				}
			}
			plat->texture.destroy_texture(plat_tex);
		}
	}

	size_t rgba_size = (size_t)width * (size_t)height * 4;
	uint8_t *rgba	 = malloc(rgba_size);
	if (rgba == NULL) {
		bapi_set_last_error("out of memory while decoding .bctex");
		return NULL;
	}
	if (bapi_bc_decode(payload, payload_size, format, width, height, rgba) != 0) {
		free(rgba);
		bapi_set_last_error("invalid .bctex payload: %s", label);
		return NULL;
	}
	bapi_texture_t tex = texture_from_pixels(label, rgba, width, height);
	free(rgba);
	return tex;
}

bapi_texture_t bapi_texture_load(const char *filepath)
{
	if (!filepath || !filepath[0]) return NULL;

	const plat_interface_t *plat = plat_get();
	if (!plat) return NULL;

	plat_surface_t *surface = NULL;
	if (plat->io.open_read && plat->io.read) {
		/* Two-stage load: read the file into memory, decode from memory. */
		size_t	 size = 0;
		uint8_t *data = bapi_file_read_alloc(filepath, &size);
		if (!data) {
			printf("[TEXTURE] Failed to read: %s\n", filepath);
			bapi_set_last_error("texture file could not be read: %s", filepath);
			return NULL;
		}
		if (texture_is_bctex(data, size)) {
			bapi_texture_t compiled = texture_from_bctex(data, size, filepath);
			free(data);
			return compiled;
		}
		if (!plat->texture.load_image_mem) {
			free(data);
			bapi_set_last_error("platform cannot decode image bytes");
			return NULL;
		}
		surface = plat->texture.load_image_mem(data, size);
		free(data);
	} else if (plat->texture.load_image) {
		surface = plat->texture.load_image(filepath);
	}

	if (!surface) {
		printf("[TEXTURE] Failed to load: %s\n", filepath);
		bapi_set_last_error("texture image could not be decoded: %s", filepath);
		return NULL;
	}

	return texture_from_surface(filepath, surface);
}

bapi_texture_t bapi_texture_load_from_memory(const void *data, size_t size)
{
	const plat_interface_t *plat = plat_get();
	if (!plat) return NULL;
	if (!data || size == 0) return NULL;
	if (texture_is_bctex(data, size)) return texture_from_bctex(data, size, "<memory>");
	if (!plat->texture.load_image_mem) return NULL;

	plat_surface_t *surface = plat->texture.load_image_mem(data, size);
	if (!surface) {
		printf("[TEXTURE] Failed to decode image from memory\n");
		bapi_set_last_error("texture image could not be decoded from memory");
		return NULL;
	}

	return texture_from_surface("<memory>", surface);
}

void bapi_texture_destroy(bapi_texture_t texture)
{
	bapi_texture_release(texture);
}

void bapi_texture_render(bapi_texture_t texture, float x, float y)
{
	if (!texture) return;
	int w = 0, h = 0;
	bapi_texture_get_size(texture, &w, &h);
	bapi_texture_render_ex(texture, x, y, (float)w, (float)h);
}

void bapi_texture_render_ex(bapi_texture_t texture, float x, float y, float w, float h)
{
	if (!texture) return;
	const plat_interface_t *plat = plat_get();
	if (!plat) return;

	if (bapi_transform_is_identity()) {
		plat->renderer.render_texture(bapi_internal_get_renderer(), texture->platform_texture, x, y,
									  w, h);
		return;
	}

	bapi_texture_draw_t params = bapi_texture_draw_default((bapi_rect_t){x, y, w, h});
	params.pivot_x			   = 0.0f;
	params.pivot_y			   = 0.0f;
	bapi_texture_draw(texture, &params);
}

bapi_texture_draw_t bapi_texture_draw_default(bapi_rect_t dst)
{
	bapi_texture_draw_t params;
	params.src.x	= 0.0f;
	params.src.y	= 0.0f;
	params.src.w	= 0.0f;
	params.src.h	= 0.0f;
	params.dst		= dst;
	params.pivot_x	= 0.5f;
	params.pivot_y	= 0.5f;
	params.rotation = 0.0f;
	params.flip		= 0;
	params.tint		= (bapi_color_t){255, 255, 255, 255};
	return params;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	if (!texture || !params) return;
	if (params->dst.w == 0.0f || params->dst.h == 0.0f) return;

	const plat_interface_t *plat = plat_get();
	if (!plat || plat->renderer.render_texture == NULL) return;

	float scale_x = bapi_transform_scale_x();
	float scale_y = bapi_transform_scale_y();
	float dst_w	  = params->dst.w * scale_x;
	float dst_h	  = params->dst.h * scale_y;
	if (dst_w < 0.0f) dst_w = -dst_w;
	if (dst_h < 0.0f) dst_h = -dst_h;
	if (dst_w <= 0.0f || dst_h <= 0.0f) return;

	bapi_vec2_t pivot =
		bapi_transform_point((bapi_vec2_t){params->dst.x + params->pivot_x * params->dst.w,
										   params->dst.y + params->pivot_y * params->dst.h});

	if (plat->renderer.render_texture_ex != NULL) {
		plat_rectf_t		src_rect;
		const plat_rectf_t *src = NULL;
		if (params->src.w > 0.0f && params->src.h > 0.0f) {
			src_rect.x = params->src.x;
			src_rect.y = params->src.y;
			src_rect.w = params->src.w;
			src_rect.h = params->src.h;
			src		   = &src_rect;
		}

		plat_rectf_t dst_rect;
		dst_rect.x = pivot.x - params->pivot_x * dst_w;
		dst_rect.y = pivot.y - params->pivot_y * dst_h;
		dst_rect.w = dst_w;
		dst_rect.h = dst_h;

		plat_pointf_t pivot_px;
		pivot_px.x = params->pivot_x * dst_w;
		pivot_px.y = params->pivot_y * dst_h;

		float rotation = bapi_transform_rotation() + params->rotation;
		plat->renderer.render_texture_ex(
			bapi_internal_get_renderer(), texture->platform_texture, src, &dst_rect, &pivot_px,
			rotation, params->flip, params->tint.r, params->tint.g, params->tint.b, params->tint.a);
		return;
	}

	/* Platforms without rotation/tint support: draw the transformed AABB. */
	float local_x[4] = {params->dst.x, params->dst.x + params->dst.w, params->dst.x + params->dst.w,
						params->dst.x};
	float local_y[4] = {params->dst.y, params->dst.y, params->dst.y + params->dst.h,
						params->dst.y + params->dst.h};
	float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
	for (int i = 0; i < 4; i++) {
		bapi_vec2_t p = bapi_transform_point((bapi_vec2_t){local_x[i], local_y[i]});
		if (i == 0) {
			min_x = max_x = p.x;
			min_y = max_y = p.y;
		} else {
			if (p.x < min_x) min_x = p.x;
			if (p.x > max_x) max_x = p.x;
			if (p.y < min_y) min_y = p.y;
			if (p.y > max_y) max_y = p.y;
		}
	}
	plat->renderer.render_texture(bapi_internal_get_renderer(), texture->platform_texture, min_x,
								  min_y, max_x - min_x, max_y - min_y);
}

void bapi_draw_image(const char *filepath, float x, float y, float w, float h)
{
	if (!filepath) return;
	bapi_texture_t texture = bapi_texture_from_file(filepath, NULL, NULL);
	if (!texture) return;
	bapi_texture_render_ex(texture, x, y, w, h);
	bapi_texture_destroy(texture);
}

void bapi_texture_get_size(bapi_texture_t texture, int *w, int *h)
{
	if (w) *w = texture ? texture->width : 0;
	if (h) *h = texture ? texture->height : 0;
}

void bapi_texture_cache_clear(void)
{
	for (int index = 0; index < BAPI_MAX_CACHED_TEXTURES; index++) {
		bapi_texture_t texture = g_texture_cache[index];
		g_texture_cache[index] = NULL;
		bapi_texture_release(texture);
	}
}

void bapi_texture_cleanup(void)
{
	memset(g_texture_cache, 0, sizeof(g_texture_cache));
	while (g_allocated_textures != NULL) {
		bapi_texture_free(g_allocated_textures);
	}
}

bapi_texture_t bapi_texture_from_file(const char *filepath, int *out_w, int *out_h)
{
	if (out_w) *out_w = 0;
	if (out_h) *out_h = 0;
	if (!filepath || !filepath[0]) return NULL;

	int free_slot = -1;
	for (int index = 0; index < BAPI_MAX_CACHED_TEXTURES; index++) {
		bapi_texture_t texture = g_texture_cache[index];
		if (!texture) {
			if (free_slot < 0) free_slot = index;
			continue;
		}
		if (strcmp(texture->cache_key, filepath) == 0) {
			texture->reference_count++;
			if (out_w) *out_w = texture->width;
			if (out_h) *out_h = texture->height;
			return texture;
		}
	}

	bapi_texture_t texture = bapi_texture_load(filepath);
	if (!texture) return NULL;
	if (free_slot < 0) {
		/* Cache is full: evict the oldest entry (round-robin). The evicted
		 * texture stays alive while user references to it remain. */
		free_slot				   = g_cache_evict_next;
		g_cache_evict_next		   = (g_cache_evict_next + 1) % BAPI_MAX_CACHED_TEXTURES;
		bapi_texture_t victim	   = g_texture_cache[free_slot];
		g_texture_cache[free_slot] = NULL;
		bapi_texture_release(victim);
	}
	if (free_slot >= 0) {
		texture->cache_key = bapi_texture_copy_key(filepath);
		if (texture->cache_key) {
			g_texture_cache[free_slot] = texture;
			texture->reference_count++;
		}
	}
	if (out_w) *out_w = texture->width;
	if (out_h) *out_h = texture->height;
	return texture;
}
