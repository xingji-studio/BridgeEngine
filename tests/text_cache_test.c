#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/render_context.h"
#include "internal/platform/platform.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct plat_font {
	float size;
};
struct plat_window {
	int unused;
};
struct plat_renderer {
	int unused;
};
struct plat_surface {
	plat_font_t	 font;
	const char	*text;
	bapi_color_t color;
};
struct plat_texture {
	plat_font_t			 font;
	char				*text;
	bapi_color_t		 color;
	struct plat_texture *next;
};

static struct plat_window	g_window;
static struct plat_renderer g_renderer;
static plat_texture_t		g_textures;
static int g_fails, g_rasters, g_creates, g_destroys, g_draws, g_measures, g_surfaces, g_fonts;
static int g_fail_surface, g_fail_texture, g_fail_metrics, g_fail_size, g_fail_font;
static int g_texture_width = 10, g_texture_height = 10;
static int g_renderer_alive, g_ttf_alive;
static const char		 *g_expected_text;
static float			  g_expected_x, g_expected_y, g_expected_size;
static bapi_color_t		  g_expected_color;
static const bapi_color_t WHITE = {255, 255, 255, 255};

static void expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		g_fails++;
	}
}

plat_renderer_t bapi_internal_get_renderer(void)
{
	return bapi_runtime_renderer();
}
void	   bapi_video_cleanup(void) {}
void	   bapi_audio_cleanup(void) {}
void	   bapi_texture_cleanup(void) {}
static int core_init(uint32_t flags)
{
	(void)flags;
	return 0;
}
static void			 core_quit(void) {}
static plat_window_t create_window(const char *title, int w, int h)
{
	(void)title;
	(void)w;
	(void)h;
	return &g_window;
}
static void destroy_window(plat_window_t window)
{
	(void)window;
}
static plat_renderer_t create_renderer(plat_window_t window)
{
	(void)window;
	g_renderer_alive = 1;
	return &g_renderer;
}
static void destroy_renderer(plat_renderer_t renderer)
{
	(void)renderer;
	expect(!g_textures && !g_fonts && !g_ttf_alive, "resources released before renderer");
	g_renderer_alive = 0;
}
static void render_noop(plat_renderer_t renderer)
{
	(void)renderer;
}
static void set_color(plat_renderer_t renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	(void)renderer;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
}
static void set_blend(plat_renderer_t renderer, plat_blend_mode_t mode)
{
	(void)renderer;
	(void)mode;
}
static int init_ttf(void)
{
	g_ttf_alive = 1;
	return 0;
}
static void quit_ttf(void)
{
	expect(!g_textures && !g_fonts, "text textures and fonts released before TTF");
	g_ttf_alive = 0;
}
static plat_font_t open_font(const char *path, float size)
{
	(void)path;
	if (g_fail_font) return NULL;
	plat_font_t font = malloc(sizeof(*font));
	if (font) {
		font->size = size;
		g_fonts++;
	}
	return font;
}
static void close_font(plat_font_t font)
{
	for (plat_texture_t t = g_textures; t; t = t->next)
		expect(t->font != font, "font eviction invalidates its text textures");
	g_fonts--;
	free(font);
}
static plat_surface_t *render_text(plat_font_t font, const char *text, int len, uint8_t r,
								   uint8_t g, uint8_t b, uint8_t a)
{
	g_rasters++;
	expect((size_t)len == strlen(text), "correct UTF-8 byte length");
	if (g_fail_surface) return NULL;
	plat_surface_t *surface = malloc(sizeof(*surface));
	if (surface) {
		*surface = (plat_surface_t){font, text, {r, g, b, a}};
		g_surfaces++;
	}
	return surface;
}
static void destroy_surface(plat_surface_t *surface)
{
	free(surface);
	g_surfaces--;
}
static plat_texture_t create_texture(plat_renderer_t renderer, plat_surface_t *surface)
{
	(void)renderer;
	if (g_fail_texture) return NULL;
	plat_texture_t texture = malloc(sizeof(*texture));
	if (!texture) return NULL;
	texture->text = malloc(strlen(surface->text) + 1);
	if (!texture->text) {
		free(texture);
		return NULL;
	}
	strcpy(texture->text, surface->text);
	texture->font  = surface->font;
	texture->color = surface->color;
	texture->next  = g_textures;
	g_textures	   = texture;
	g_creates++;
	return texture;
}
static void destroy_texture(plat_texture_t texture)
{
	expect(g_renderer_alive, "texture released while renderer is alive");
	plat_texture_t *p = &g_textures;
	while (*p && *p != texture) p = &(*p)->next;
	expect(*p == texture, "texture released exactly once");
	if (*p) *p = texture->next;
	free(texture->text);
	free(texture);
	g_destroys++;
}
static int texture_size(plat_texture_t texture, int *w, int *h)
{
	(void)texture;
	*w = g_texture_width;
	*h = g_texture_height;
	return g_fail_size ? -1 : 0;
}
static int measure(plat_font_t font, const char *text, int len, int *w, int *h)
{
	(void)font;
	(void)text;
	(void)len;
	g_measures++;
	if (g_fail_metrics) return -1;
	*w = 20;
	*h = 30;
	return 0;
}
static void render_texture(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
						   float w, float h)
{
	expect(renderer == &g_renderer && g_renderer_alive, "live renderer used");
	expect(strcmp(texture->text, g_expected_text) == 0, "cached text content is correct");
	expect(texture->font->size == g_expected_size, "quantized font size is correct");
	expect(texture->color.r == g_expected_color.r && texture->color.g == g_expected_color.g &&
			   texture->color.b == g_expected_color.b && texture->color.a == g_expected_color.a,
		   "cached RGBA is correct");
	expect(x == g_expected_x && y == g_expected_y && w == 20 && h == 30,
		   "position stays dynamic and layout uses text metrics");
	g_draws++;
}
static void draw(const char *text, float size, bapi_color_t color, float x, float y)
{
	g_expected_text	 = text;
	g_expected_size	 = size <= 1 ? 1 : (float)(int)(size + 0.5f);
	g_expected_color = color;
	g_expected_x	 = x;
	g_expected_y	 = y;
	bapi_draw_text(text, x, y, size, color);
}
static void clear_cache(void)
{
	bapi_text_cleanup();
	bapi_text_cleanup();
	expect(!g_textures && !g_surfaces && !g_fonts, "cleanup is complete and idempotent");
}
static void test_reuse(void)
{
	int	 before = g_creates, measured = g_measures;
	char buffer[] = "label";
	for (int i = 0; i < 1000; i++) draw(buffer, 16.1f, WHITE, (float)i, 7);
	expect(g_creates == before + 1 && g_measures == measured + 1,
		   "1000 repeated labels rasterize, upload and measure once");
	strcpy(buffer, "other");
	draw(buffer, 16.1f, WHITE, 1, 2);
	draw("label", 16.4f, WHITE, 3, 4);
	expect(g_creates == before + 2, "cache owns text bytes and shares quantized sizes");
	float w = 0, h = 0;
	measured = g_measures;
	bapi_get_text_size("label", 16.2f, &w, &h);
	expect(w == 20 && h == 30 && g_measures == measured, "drawn text reuses metrics");
	const bapi_color_t colors[] = {
		{254, 255, 255, 255}, {255, 254, 255, 255}, {255, 255, 254, 255}, {255, 255, 255, 254}};
	for (int i = 0; i < 4; i++) draw("label", 16, colors[i], 0, 0);
	expect(g_creates == before + 6, "each color channel participates in cache key");
	draw("label", 17, WHITE, 0, 0);
	draw("中文", 16, WHITE, 0, 0);
	clear_cache();
}
static void test_eviction(void)
{
	char text[32];
	for (int i = 0; i < BAPI_TEXT_CACHE_CAPACITY; i++) {
		snprintf(text, sizeof(text), "label-%d", i);
		draw(text, 16, WHITE, 0, 0);
	}
	draw("label-0", 16, WHITE, 0, 0);
	int before = g_creates;
	draw("replacement", 16, WHITE, 0, 0);
	draw("label-0", 16, WHITE, 0, 0);
	expect(g_creates == before + 1, "recently drawn entry survives capacity eviction");
	draw("label-1", 16, WHITE, 0, 0);
	expect(g_creates == before + 2, "least recently used entry is evicted");
	clear_cache();

	g_texture_width = g_texture_height = 20; /* two 1600-byte textures fit, three do not */
	draw("one", 16, WHITE, 0, 0);
	draw("two", 16, WHITE, 0, 0);
	before = g_destroys;
	draw("three", 16, WHITE, 0, 0);
	expect(g_destroys == before + 1, "byte budget evicts before entry capacity is reached");
	clear_cache();

	g_texture_width = g_texture_height = 10;
	draw("small-one", 16, WHITE, 0, 0);
	draw("small-two", 16, WHITE, 0, 0);
	draw("small-three", 16, WHITE, 0, 0);
	g_texture_width = g_texture_height = 31;
	before							   = g_destroys;
	draw("large", 16, WHITE, 0, 0);
	expect(g_destroys == before + 3,
		   "one insertion can evict multiple entries for its byte budget");
	clear_cache();
	g_texture_width = g_texture_height = INT_MAX;
	before							   = g_destroys;
	draw("oversized", 16, WHITE, 0, 0);
	draw("oversized", 16, WHITE, 0, 0);
	expect(g_destroys == before + 2, "oversized textures draw uncached without size overflow");
	g_texture_width = g_texture_height = 10;
	clear_cache();

	draw("old font", 1, WHITE, 0, 0);
	for (int i = 2; i <= 64; i++) bapi_get_text_size("metrics", (float)i, NULL, NULL);
	before		= g_destroys;
	g_fail_font = 1;
	bapi_get_text_size("evict", 65, NULL, NULL);
	expect(g_destroys == before + 1,
		   "font eviction releases dependent textures even on load failure");
	g_fail_font = 0;
	draw("old font", 1, WHITE, 0, 0);
	clear_cache();
}
static void test_failures(void)
{
	int before	   = g_draws;
	g_fail_surface = 1;
	draw("failure", 16, WHITE, 0, 0);
	g_fail_surface = 0;
	g_fail_texture = 1;
	draw("failure", 16, WHITE, 0, 0);
	g_fail_texture = 0;
	g_fail_metrics = 1;
	draw("failure", 16, WHITE, 0, 0);
	g_fail_metrics = 0;
	expect(g_draws == before && !g_surfaces && !g_textures, "failed creation leaves no resources");
	draw("failure", 16, WHITE, 0, 0);
	expect(g_draws == before + 1, "failed text can be retried");
	clear_cache();
	g_fail_size = 1;
	draw("unknown size", 16, WHITE, 0, 0);
	expect(!g_textures, "unknown texture size falls back to uncached drawing");
	g_fail_size = 0;
	before		= g_draws;
	bapi_draw_text(NULL, 0, 0, 16, WHITE);
	bapi_draw_text("", 0, 0, 16, WHITE);
	bapi_draw_text("invalid", 0, 0, NAN, WHITE);
	bapi_draw_text("invalid", 0, 0, INFINITY, WHITE);
	bapi_draw_text("invalid", 0, 0, (float)INT_MAX, WHITE);
	expect(g_draws == before, "empty and invalid-size text is ignored");
	draw("small", -1e30f, WHITE, 0, 0);
	clear_cache();
}
int main(int argc, char **argv)
{
	const plat_interface_t platform = {
		.core	  = {.init = core_init, .quit = core_quit},
		.window	  = {.create_window = create_window, .destroy_window = destroy_window},
		.renderer = {.create_renderer			 = create_renderer,
					 .destroy_renderer			 = destroy_renderer,
					 .set_render_draw_color		 = set_color,
					 .set_render_draw_blend_mode = set_blend,
					 .render_clear				 = render_noop,
					 .render_present			 = render_noop,
					 .render_texture			 = render_texture},
		.texture  = {.create_texture_from_surface = create_texture,
					 .destroy_texture			  = destroy_texture,
					 .destroy_surface			  = destroy_surface,
					 .get_texture_size			  = texture_size},
		.text	  = {.init_ttf			  = init_ttf,
					 .quit_ttf			  = quit_ttf,
					 .open_font			  = open_font,
					 .close_font		  = close_font,
					 .render_text_blended = render_text,
					 .get_string_size	  = measure},
	};
	if (bapi_runtime_start(&platform, "cache", 100, 100) != 0) return 1;
	if (argc == 2 && strcmp(argv[1], "--benchmark") == 0) {
		const char *labels[] = {"Play", "Settings", "Back", "Quit"};
		clock_t		start	 = clock();
		for (int frame = 0; frame < 10000; frame++)
			for (int i = 0; i < 4; i++) draw(labels[i], 16, WHITE, (float)i, (float)frame);
		printf("mock text: draws=%d rasters=%d creates=%d measures=%d seconds=%.6f\n", g_draws,
			   g_rasters, g_creates, g_measures, (double)(clock() - start) / CLOCKS_PER_SEC);
	} else {
		test_reuse();
		test_eviction();
		test_failures();
		draw("shutdown", 16, WHITE, 0, 0);
		bapi_runtime_stop();
		bapi_draw_text("stopped", 0, 0, 16, WHITE);
		if (bapi_runtime_start(&platform, "restart", 100, 100) != 0) return 1;
		int before = g_creates;
		draw("shutdown", 16, WHITE, 0, 0);
		expect(g_creates == before + 1, "restart does not reuse stale renderer textures");
	}
	bapi_runtime_stop();
	expect(g_creates == g_destroys && !g_surfaces && !g_fonts, "all text resources released");
	return g_fails ? 1 : 0;
}
