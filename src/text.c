#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/render_context.h"
#include "internal/platform/platform.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FONTS		  64
#define FONT_PATH_RUNTIME "assets/text/font.ttf"
#define FONT_PATH_SOURCE  "examples/assets/text/font.ttf"
#define FONT_PATH_LEGACY  "text/font.ttf"

/* Bound both metadata and estimated RGBA texture storage. XJ380 has a much
 * smaller memory budget; its text textures still use the same ownership API. */
#ifndef BAPI_TEXT_CACHE_CAPACITY
#ifdef USE_BACKEND_XJ380
#define BAPI_TEXT_CACHE_CAPACITY 32
#else
#define BAPI_TEXT_CACHE_CAPACITY 128
#endif
#endif
#ifndef BAPI_TEXT_CACHE_BYTES
#ifdef USE_BACKEND_XJ380
#define BAPI_TEXT_CACHE_BYTES (256u * 1024u)
#else
#define BAPI_TEXT_CACHE_BYTES (4u * 1024u * 1024u)
#endif
#endif

typedef struct {
	plat_font_t font;
	float		size;
	int			in_use;
	uint64_t	last_used;
} cached_font_t;

static cached_font_t g_font_cache[MAX_FONTS] = {0};
static uint64_t		 g_font_clock			 = 0;

typedef struct {
	plat_font_t	   font;
	plat_texture_t texture;
	char		  *text;
	size_t		   length;
	size_t		   bytes;
	bapi_color_t   color;
	int			   width, height;
	uint64_t	   last_used;
} cached_text_t;

static cached_text_t g_text_cache[BAPI_TEXT_CACHE_CAPACITY];
static size_t		 g_text_cache_bytes;
static uint64_t		 g_text_clock;

static void text_cache_release(cached_text_t *entry)
{
	const plat_interface_t *plat = plat_get();
	if (entry->texture && plat) plat->texture.destroy_texture(entry->texture);
	free(entry->text);
	g_text_cache_bytes -= entry->bytes;
	memset(entry, 0, sizeof(*entry));
}

static void text_cache_remove_font(plat_font_t font)
{
	for (int i = 0; i < BAPI_TEXT_CACHE_CAPACITY; i++) {
		if (g_text_cache[i].font == font) text_cache_release(&g_text_cache[i]);
	}
}

/* A NULL color requests cached metrics regardless of the rasterized color. */
static cached_text_t *text_cache_find(plat_font_t font, const char *text, size_t length,
									  const bapi_color_t *color)
{
	for (int i = 0; i < BAPI_TEXT_CACHE_CAPACITY; i++) {
		cached_text_t *entry = &g_text_cache[i];
		if (entry->font != font || entry->length != length || !entry->text) continue;
		if (color && (entry->color.r != color->r || entry->color.g != color->g ||
					  entry->color.b != color->b || entry->color.a != color->a))
			continue;
		if (memcmp(entry->text, text, length) != 0) continue;
		entry->last_used = ++g_text_clock;
		return entry;
	}
	return NULL;
}

/* Takes ownership only on success. Uncacheable text is still drawn normally. */
static int text_cache_store(plat_font_t font, plat_texture_t texture, const char *text,
							size_t length, bapi_color_t color, int width, int height)
{
	const plat_interface_t *plat		  = plat_get();
	int						texture_width = 0, texture_height = 0;
	if (!plat->texture.get_texture_size ||
		plat->texture.get_texture_size(texture, &texture_width, &texture_height) != 0 ||
		texture_width <= 0 || texture_height <= 0)
		return 0;
	/* Check before multiplying, including on 32-bit targets. */
	size_t budget = BAPI_TEXT_CACHE_BYTES;
	if ((size_t)texture_width > budget / 4u / (size_t)texture_height) return 0;
	size_t bytes = (size_t)texture_width * (size_t)texture_height * 4u;
	if (length >= budget - bytes) return 0;
	bytes += length + 1;
	char *copy = malloc(length + 1);
	if (!copy) return 0;
	memcpy(copy, text, length + 1);

	int slot = -1;
	while (slot < 0 || g_text_cache_bytes > budget - bytes) {
		int oldest = -1;
		for (int i = 0; i < BAPI_TEXT_CACHE_CAPACITY; i++) {
			if (!g_text_cache[i].texture) {
				if (slot < 0) slot = i;
			} else if (oldest < 0 || g_text_cache[i].last_used < g_text_cache[oldest].last_used) {
				oldest = i;
			}
		}
		if (slot >= 0 && g_text_cache_bytes <= budget - bytes) break;
		text_cache_release(&g_text_cache[oldest]);
		if (slot < 0) slot = oldest;
	}
	g_text_cache[slot] =
		(cached_text_t){font, texture, copy, length, bytes, color, width, height, ++g_text_clock};
	g_text_cache_bytes += bytes;
	return 1;
}

// Quantize to integer pixels so zooming reuses cached fonts instead of
// creating a new font for every fractional size.
static float font_quantize_size(float size)
{
	if (size <= 1.0f) return 1.0f;
	/* Reject NaN/infinity and sizes outside the integer conversion range. */
	if (!(size < (float)INT_MAX)) return 0.0f;
	return (float)(int)(size + 0.5f);
}

static plat_font_t get_or_load_font(float size)
{
	if (!bapi_runtime_is_text_initialized()) bapi_text_init();
	if (!bapi_runtime_is_text_initialized()) return NULL;

	const plat_interface_t *plat = plat_get();

	size = font_quantize_size(size);
	if (size == 0.0f) return NULL;

	for (int i = 0; i < MAX_FONTS; i++) {
		if (g_font_cache[i].in_use && g_font_cache[i].size == size) {
			g_font_cache[i].last_used = ++g_font_clock;
			return g_font_cache[i].font;
		}
	}

	// prefer a free slot; otherwise reuse the least-recently-used one so the
	// cache can never silently starve rendering (which made all text vanish
	// when zooming changed every requested size at once)
	int slot = -1;
	for (int i = 0; i < MAX_FONTS; i++) {
		if (!g_font_cache[i].in_use) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		int oldest = 0;
		for (int i = 1; i < MAX_FONTS; i++) {
			if (g_font_cache[i].last_used < g_font_cache[oldest].last_used) oldest = i;
		}
		slot = oldest;
		if (g_font_cache[slot].font) {
			text_cache_remove_font(g_font_cache[slot].font);
			plat->text.close_font(g_font_cache[slot].font);
			g_font_cache[slot].font = NULL;
		}
	}

	const char *font_paths[] = {FONT_PATH_RUNTIME, FONT_PATH_SOURCE, FONT_PATH_LEGACY};
	for (size_t path_index = 0; path_index < sizeof(font_paths) / sizeof(font_paths[0]);
		 path_index++) {
		g_font_cache[slot].font = plat->text.open_font(font_paths[path_index], size);
		if (g_font_cache[slot].font) {
			g_font_cache[slot].size		 = size;
			g_font_cache[slot].in_use	 = 1;
			g_font_cache[slot].last_used = ++g_font_clock;
			return g_font_cache[slot].font;
		}
	}

	g_font_cache[slot].in_use = 0;
	return NULL;
}

void bapi_text_init(void)
{
	if (!bapi_runtime_is_initialized() || bapi_runtime_is_text_initialized()) return;
	memset(g_font_cache, 0, sizeof(g_font_cache));
	g_font_clock = 0;
	g_text_clock = 0;
	bapi_runtime_set_text_initialized(true);
}

void bapi_text_cleanup(void)
{
	const plat_interface_t *plat = plat_get();
	if (!bapi_runtime_is_text_initialized()) return;
	for (int i = 0; i < BAPI_TEXT_CACHE_CAPACITY; i++) text_cache_release(&g_text_cache[i]);
	for (int i = 0; i < MAX_FONTS; i++) {
		if (plat && g_font_cache[i].in_use && g_font_cache[i].font) {
			plat->text.close_font(g_font_cache[i].font);
		}
	}
	memset(g_font_cache, 0, sizeof(g_font_cache));
	bapi_runtime_set_text_initialized(false);
}

void bapi_draw_text(const char *text, float x, float y, float size, bapi_color_t color)
{
	if (!text || !text[0]) return;
	if (!bapi_runtime_is_initialized()) return;
	size_t length = strlen(text);
	if (length > INT_MAX) return;

	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();

	plat_font_t font = get_or_load_font(size);
	if (!font) {
		return;
	}
	cached_text_t *cached = text_cache_find(font, text, length, &color);
	if (cached) {
		plat->renderer.render_texture(renderer, cached->texture, x, y, (float)cached->width,
									  (float)cached->height);
		return;
	}

	plat_surface_t *surface =
		plat->text.render_text_blended(font, text, (int)length, color.r, color.g, color.b, color.a);
	if (!surface) {
		return;
	}

	plat_texture_t texture = plat->texture.create_texture_from_surface(renderer, surface);
	if (!texture) {
		plat->texture.destroy_surface(surface);
		return;
	}

	int surf_w = 0, surf_h = 0;
	cached = text_cache_find(font, text, length, NULL);
	if (cached) {
		surf_w = cached->width;
		surf_h = cached->height;
	} else if (plat->text.get_string_size(font, text, (int)length, &surf_w, &surf_h) != 0) {
		plat->texture.destroy_texture(texture);
		plat->texture.destroy_surface(surface);
		return;
	}

	plat->renderer.render_texture(renderer, texture, x, y, (float)surf_w, (float)surf_h);

	plat->texture.destroy_surface(surface);
	if (!text_cache_store(font, texture, text, length, color, surf_w, surf_h)) {
		plat->texture.destroy_texture(texture);
	}
}

void bapi_get_text_size(const char *text, float size, float *width, float *height)
{
	if (width) *width = 0;
	if (height) *height = 0;

	if (!text || !text[0]) return;
	if (!bapi_runtime_is_initialized()) return;
	size_t length = strlen(text);
	if (length > INT_MAX) return;

	const plat_interface_t *plat = plat_get();

	plat_font_t font = get_or_load_font(size);
	if (!font) return;

	int			   w = 0, h = 0;
	cached_text_t *cached = text_cache_find(font, text, length, NULL);
	if (cached) {
		w = cached->width;
		h = cached->height;
	} else {
		plat->text.get_string_size(font, text, (int)length, &w, &h);
	}

	if (width) *width = (float)w;
	if (height) *height = (float)h;
}
