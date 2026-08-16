/* MiniOS backend for BridgeEngine.
 *
 * Renders into the window-manager framebuffer (XRGB8888):
 *   - window  = WM window with a client-side pixel buffer
 *   - renderer primitives draw directly into that buffer; present() flushes
 *   - text is rasterized from the built-in 8x16 VGA font
 *   - audio/video are unsupported; engine falls back via capabilities
 */
#include "internal/platform/platform.h"
#include "internal/platform/platform_types.h"
#include "font8x16_data.h"

#ifdef USE_BACKEND_MINIOS

#include <minios/abi.h>
#include <minios/gui.h>
#include <minios/minios.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct plat_window {
	int			 id;
	int			 width;
	int			 height;
	unsigned int *pixels;
};

struct plat_renderer {
	plat_window_t	  window;
	uint8_t			  r, g, b, a;
	plat_blend_mode_t blend_mode;
};

struct plat_texture {
	int		  width;
	int		  height;
	uint8_t *pixels;
	int		  pitch;
};

struct plat_font {
	float size;
};

struct plat_audio_device {
	int dummy;
};

struct plat_audio_stream {
	int dummy;
};

struct plat_mutex {
	volatile int flag;
};

struct plat_io {
	FILE *fp;
};

struct plat_surface {
	int		  width;
	int		  height;
	int		  pitch;
	uint8_t *pixels;
};

static plat_window_t g_current_window = NULL;

static float g_mouse_x = 0.0f;
static float g_mouse_y = 0.0f;

static uint64_t g_start_ms = 0;

static inline uint32_t minios_make_pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	(void)a;
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static char *minios_copy_string_len(const char *text, int len)
{
	if (!text) return NULL;
	size_t copy_len = (len >= 0) ? (size_t)len : strlen(text);
	char  *copy		= (char *)malloc(copy_len + 1);
	if (copy) {
		memcpy(copy, text, copy_len);
		copy[copy_len] = '\0';
	}
	return copy;
}

/* ---- core ---- */

static int minios_init(uint32_t flags)
{
	(void)flags;
	g_start_ms = uptime_ms();
	return 0;
}

static void minios_quit(void) {}

static void minios_delay(uint32_t ms)
{
	sleep_ms(ms);
}

static uint32_t minios_get_ticks(void)
{
	return (uint32_t)(uptime_ms() - g_start_ms);
}

/* ---- window ---- */

static plat_window_t minios_create_window(const char *title, int width, int height)
{
	plat_window_t w = (plat_window_t)malloc(sizeof(struct plat_window));
	if (!w) return NULL;

	struct minios_win win;
	if (win_create(width, height, title ? title : "", &win) < 0) {
		free(w);
		return NULL;
	}

	w->id	   = win.id;
	w->width   = win.w;
	w->height  = win.h;
	w->pixels  = win.pixels;
	g_current_window = w;
	return w;
}

static void minios_destroy_window(plat_window_t window)
{
	if (!window) return;
	if (g_current_window == window) g_current_window = NULL;
	win_destroy(window->id);
	free(window);
}

static void minios_get_mouse_state(float *x, float *y)
{
	if (x) *x = g_mouse_x;
	if (y) *y = g_mouse_y;
}

static uint32_t minios_map_key(uint32_t code)
{
	switch (code) {
	case 27:
		return PLAT_KEY_ESC;
	case 8:
		return PLAT_KEY_BACKSPACE;
	case 9:
		return PLAT_KEY_TAB;
	case 13:
	case 10:
		return PLAT_KEY_ENTER;
	case 32:
		return PLAT_KEY_SPACE;
	case MINIOS_KEY_F1:
		return PLAT_KEY_F1;
	case MINIOS_KEY_F2:
		return PLAT_KEY_F2;
	case MINIOS_KEY_F3:
		return PLAT_KEY_F3;
	case MINIOS_KEY_F4:
		return PLAT_KEY_F4;
	case MINIOS_KEY_F5:
		return PLAT_KEY_F5;
	case MINIOS_KEY_F6:
		return PLAT_KEY_F6;
	case MINIOS_KEY_F7:
		return PLAT_KEY_F7;
	case MINIOS_KEY_F8:
		return PLAT_KEY_F8;
	case MINIOS_KEY_F9:
		return PLAT_KEY_F9;
	case MINIOS_KEY_F10:
		return PLAT_KEY_F10;
	case MINIOS_KEY_F11:
		return PLAT_KEY_F11;
	case MINIOS_KEY_F12:
		return PLAT_KEY_F12;
	default:
		return code;
	}
}

static int minios_poll_event(plat_event_t *event)
{
	if (!event || !g_current_window) return 0;

	struct minios_event ev;
	if (win_event(g_current_window->id, &ev, 0) < 0) {
		return 0;
	}

	memset(event, 0, sizeof(*event));

	if (ev.code < 0x100) {
		if (ev.code == MINIOS_EV_CLOSE) {
			event->type = PLAT_EVENT_QUIT;
		} else {
			event->type		 = PLAT_EVENT_KEY_DOWN;
			event->data.key.key = minios_map_key((uint32_t)ev.code);
		}
		return 1;
	}

	switch (ev.code) {
	case MINIOS_EV_CLOSE:
		event->type = PLAT_EVENT_QUIT;
		break;
	case MINIOS_EV_KEY_UP:
		event->type		  = PLAT_EVENT_KEY_UP;
		event->data.key.key = minios_map_key((uint32_t)ev.mx);
		break;
	case MINIOS_EV_MOUSE_DOWN:
		event->type				 = PLAT_EVENT_MOUSE_BUTTON_DOWN;
		event->data.button.x	 = (float)ev.mx;
		event->data.button.y	 = (float)ev.my;
		event->data.button.button = ev.alt ? PLAT_BUTTON_RIGHT : PLAT_BUTTON_LEFT;
		break;
	case MINIOS_EV_MOUSE_UP:
		event->type				 = PLAT_EVENT_MOUSE_BUTTON_UP;
		event->data.button.x	 = (float)ev.mx;
		event->data.button.y	 = (float)ev.my;
		event->data.button.button = ev.alt ? PLAT_BUTTON_RIGHT : PLAT_BUTTON_LEFT;
		break;
	case MINIOS_EV_MOUSE_MOVE:
		event->type			= PLAT_EVENT_MOUSE_MOTION;
		event->data.motion.x = (float)ev.mx;
		event->data.motion.y = (float)ev.my;
		g_mouse_x			= (float)ev.mx;
		g_mouse_y			= (float)ev.my;
		break;
	default:
		if (ev.code >= MINIOS_KEY_UP && ev.code <= MINIOS_KEY_DEL) {
			event->type		 = PLAT_EVENT_KEY_DOWN;
			event->data.key.key = minios_map_key((uint32_t)ev.code);
		} else {
			return 0;
		}
		break;
	}
	return 1;
}

/* ---- renderer ---- */

static void minios_put_pixel(plat_renderer_t r, int x, int y, uint32_t color)
{
	plat_window_t w = r->window;
	if (x < 0 || y < 0 || x >= w->width || y >= w->height) {
		return;
	}
	w->pixels[(uint32_t)y * (uint32_t)w->width + (uint32_t)x] = color;
}

static plat_renderer_t minios_create_renderer(plat_window_t window)
{
	if (!window) return NULL;
	plat_renderer_t r = malloc(sizeof(struct plat_renderer));
	if (!r) return NULL;
	r->window	  = window;
	r->r		  = 0;
	r->g		  = 0;
	r->b		  = 0;
	r->a		  = 255;
	r->blend_mode = PLAT_BLENDMODE_BLEND;
	return r;
}

static void minios_destroy_renderer(plat_renderer_t renderer)
{
	if (renderer) free(renderer);
}

static void minios_set_render_draw_color(plat_renderer_t renderer, uint8_t r, uint8_t g, uint8_t b,
										 uint8_t a)
{
	if (!renderer) return;
	renderer->r = r;
	renderer->g = g;
	renderer->b = b;
	renderer->a = a;
}

static void minios_set_render_draw_blend_mode(plat_renderer_t renderer, plat_blend_mode_t mode)
{
	if (renderer) renderer->blend_mode = mode;
}

static void minios_render_clear(plat_renderer_t renderer)
{
	if (!renderer || !renderer->window) return;
	plat_window_t w = renderer->window;
	uint32_t	  color = minios_make_pixel(renderer->r, renderer->g, renderer->b, renderer->a);
	for (int i = 0; i < w->width * w->height; i++) {
		w->pixels[i] = color;
	}
}

static void minios_render_present(plat_renderer_t renderer)
{
	if (!renderer || !renderer->window) return;
	win_flush(renderer->window->id);
}

static void minios_render_point(plat_renderer_t renderer, float x, float y)
{
	if (!renderer || !renderer->window) return;
	uint32_t color = minios_make_pixel(renderer->r, renderer->g, renderer->b, renderer->a);
	minios_put_pixel(renderer, (int)x, (int)y, color);
}

static void minios_render_line(plat_renderer_t renderer, float x1, float y1, float x2, float y2)
{
	if (!renderer || !renderer->window) return;
	plat_window_t w = renderer->window;
	uint32_t color = minios_make_pixel(renderer->r, renderer->g, renderer->b, renderer->a);

	int x0 = (int)x1, y0 = (int)y1, x1i = (int)x2, y1i = (int)y2;
	int dx = x1i - x0 > 0 ? x1i - x0 : -(x1i - x0);
	int dy = y1i - y0 > 0 ? y1i - y0 : -(y1i - y0);
	int sx = x0 < x1i ? 1 : -1;
	int sy = y0 < y1i ? 1 : -1;
	int err = dx - dy;

	for (;;) {
		minios_put_pixel(renderer, x0, y0, color);
		if (x0 == x1i && y0 == y1i) break;
		int e2 = err * 2;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
	(void)w;
}

static void minios_render_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	if (!renderer || !renderer->window) return;
	uint32_t color = minios_make_pixel(renderer->r, renderer->g, renderer->b, renderer->a);
	int		 cx		= (int)x, cy = (int)y;
	int		 cw = (int)w, ch = (int)h;
	for (int i = 0; i < cw; i++) {
		minios_put_pixel(renderer, cx + i, cy, color);
		minios_put_pixel(renderer, cx + i, cy + ch - 1, color);
	}
	for (int j = 0; j < ch; j++) {
		minios_put_pixel(renderer, cx, cy + j, color);
		minios_put_pixel(renderer, cx + cw - 1, cy + j, color);
	}
}

static void minios_render_fill_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	if (!renderer || !renderer->window) return;
	uint32_t color = minios_make_pixel(renderer->r, renderer->g, renderer->b, renderer->a);
	for (int j = (int)y; j < (int)y + (int)h; j++) {
		for (int i = (int)x; i < (int)x + (int)w; i++) {
			minios_put_pixel(renderer, i, j, color);
		}
	}
}

static uint32_t minios_argb_lerp(uint32_t dst, uint32_t src, uint8_t a)
{
	if (a >= 255) return src;
	if (a == 0) return dst;
	uint32_t inv = 255 - a;
	uint8_t	r = (uint8_t)((((dst >> 16) & 0xFF) * inv + ((src >> 16) & 0xFF) * a) / 255);
	uint8_t	g = (uint8_t)((((dst >> 8) & 0xFF) * inv + ((src >> 8) & 0xFF) * a) / 255);
	uint8_t	b = (uint8_t)(((dst & 0xFF) * inv + (src & 0xFF) * a) / 255);
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void minios_blit_texture(plat_renderer_t renderer, plat_texture_t texture, int dx, int dy,
								int dw, int dh)
{
	for (int j = 0; j < dh; j++) {
		int sy = texture->height > 1 ? j * texture->height / dh : 0;
		for (int i = 0; i < dw; i++) {
			int	sx = texture->width > 1 ? i * texture->width / dw : 0;
			uint8_t src_a = texture->pixels[sy * texture->pitch + sx * 4 + 3];
			uint32_t src = ((uint32_t)texture->pixels[sy * texture->pitch + sx * 4] << 16) |
						   ((uint32_t)texture->pixels[sy * texture->pitch + sx * 4 + 1] << 8) |
						   (uint32_t)texture->pixels[sy * texture->pitch + sx * 4 + 2];
			if (src_a == 255) {
				minios_put_pixel(renderer, dx + i, dy + j, src);
			} else if (src_a != 0 && renderer->window) {
				int px = dx + i, py = dy + j;
				if (px >= 0 && py >= 0 && px < renderer->window->width &&
					py < renderer->window->height) {
					renderer->window->pixels[(uint32_t)py * (uint32_t)renderer->window->width +
											 (uint32_t)px] =
						minios_argb_lerp(renderer->window->pixels[(uint32_t)py *
																  (uint32_t)renderer->window->width +
																  (uint32_t)px],
										 src, src_a);
				}
			}
		}
	}
}

static void minios_render_texture(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
								  float w, float h)
{
	if (!renderer || !texture || !renderer->window) return;
	int dw = (w > 0) ? (int)w : texture->width;
	int dh = (h > 0) ? (int)h : texture->height;
	minios_blit_texture(renderer, texture, (int)x, (int)y, dw, dh);
}

/* ---- texture ---- */

static plat_texture_t minios_create_texture_from_surface(plat_renderer_t renderer,
														 plat_surface_t *surface)
{
	(void)renderer;
	if (!surface) return NULL;
	plat_texture_t t = malloc(sizeof(struct plat_texture));
	if (!t) return NULL;
	t->width	= surface->width;
	t->height	= surface->height;
	t->pitch	= surface->pitch;
	t->pixels	= malloc((size_t)(t->width * t->height * 4));
	if (!t->pixels) {
		free(t);
		return NULL;
	}
	memcpy(t->pixels, surface->pixels, (size_t)(t->width * t->height * 4));
	return t;
}

static plat_texture_t minios_create_texture(plat_renderer_t renderer, plat_pixel_format_t format,
											plat_texture_access_t access, int width, int height)
{
	(void)renderer;
	(void)format;
	(void)access;
	plat_texture_t t = malloc(sizeof(struct plat_texture));
	if (!t) return NULL;
	t->width	= width;
	t->height	= height;
	t->pitch	= width * 4;
	t->pixels	= malloc((size_t)(width * height * 4));
	if (!t->pixels) {
		free(t);
		return NULL;
	}
	memset(t->pixels, 0, (size_t)(width * height * 4));
	return t;
}

static int minios_update_texture(plat_texture_t texture, const void *pixels, int pitch)
{
	if (!texture || !pixels) return -1;
	for (int row = 0; row < texture->height; row++) {
		memcpy(texture->pixels + (size_t)row * texture->pitch,
			   (const uint8_t *)pixels + (size_t)row * pitch, (size_t)texture->pitch);
	}
	return 0;
}

static int minios_get_texture_size(plat_texture_t texture, int *width, int *height)
{
	if (!texture || !width || !height) return -1;
	*width	= texture->width;
	*height = texture->height;
	return 0;
}

static void minios_destroy_texture(plat_texture_t texture)
{
	if (!texture) return;
	if (texture->pixels) free(texture->pixels);
	free(texture);
}

static plat_surface_t *minios_load_image(const char *filepath)
{
	(void)filepath;
	return NULL;
}

static void minios_destroy_surface(plat_surface_t *surface)
{
	if (!surface) return;
	if (surface->pixels) free(surface->pixels);
	free(surface);
}

/* ---- text ---- */

static int minios_init_ttf(void)
{
	return 0;
}

static void minios_quit_ttf(void) {}

static plat_font_t minios_open_font(const char *filepath, float size)
{
	(void)filepath;
	plat_font_t f = malloc(sizeof(struct plat_font));
	if (!f) return NULL;
	f->size = size > 0 ? size : 16.0f;
	return f;
}

static void minios_close_font(plat_font_t font)
{
	if (font) free(font);
}

static int minios_font_scale(float size)
{
	int s = (int)(size / 8.0f + 0.5f);
	return s < 1 ? 1 : (s > 4 ? 4 : s);
}

static plat_surface_t *minios_render_text_blended(plat_font_t font, const char *text, int len,
												  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (!font || !text) return NULL;

	char *copy = minios_copy_string_len(text, len);
	if (!copy) return NULL;

	size_t tlen = strlen(copy);
	int		scale = minios_font_scale(font->size);
	int		gw	  = 8 * scale;
	int		gh	  = 16 * scale;
	int		width = (int)tlen * gw;
	int		height = gh;
	if (width < 1) width = 1;

	plat_surface_t *s = malloc(sizeof(plat_surface_t));
	if (!s) {
		free(copy);
		return NULL;
	}
	s->width	= width;
	s->height	= height;
	s->pitch	= width * 4;
	s->pixels	= malloc((size_t)width * (size_t)height * 4);
	if (!s->pixels) {
		free(s);
		free(copy);
		return NULL;
	}
	memset(s->pixels, 0, (size_t)width * (size_t)height * 4);

	for (size_t c = 0; c < tlen; c++) {
		unsigned char ch = (unsigned char)copy[c];
		if (ch < 32 || ch > 126) {
			ch = '?';
		}
		const uint8_t *glyph = minios_font8x16[ch - 32];
		for (int row = 0; row < 16; row++) {
			for (int col = 0; col < 8; col++) {
				if (!(glyph[row] & (0x80 >> col))) continue;
				for (int sy = 0; sy < scale; sy++) {
					for (int sx = 0; sx < scale; sx++) {
						int		 tx = (int)c * gw + col * scale + sx;
						int		 ty = row * scale + sy;
						uint8_t *px = s->pixels + (size_t)ty * s->pitch + (size_t)tx * 4;
						px[0] = r;
						px[1] = g;
						px[2] = b;
						px[3] = a;
					}
				}
			}
		}
	}
	free(copy);
	return s;
}

static int minios_get_string_size(plat_font_t font, const char *text, int len, int *w, int *h)
{
	if (!font || !text) return -1;
	int		scale = minios_font_scale(font->size);
	size_t	tlen	= 0;
	if (len >= 0) {
		tlen = 0;
		for (int i = 0; i < len && text[i]; i++) tlen++;
	} else {
		tlen = strlen(text);
	}
	if (w) *w = (int)tlen * 8 * scale;
	if (h) *h = 16 * scale;
	return 0;
}

/* ---- audio: unsupported (kept for vtable-parity, never referenced) ---- */

#define MINIOS_AUDIO_STUB __attribute__((unused))

static MINIOS_AUDIO_STUB plat_audio_device_t minios_open_audio_device(plat_audio_format_t format, int channels,
													int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return NULL;
}

static MINIOS_AUDIO_STUB void minios_close_audio_device(plat_audio_device_t device) { (void)device; }
static MINIOS_AUDIO_STUB plat_audio_stream_t minios_create_audio_stream(const plat_audio_spec_t *spec)
{
	(void)spec;
	return NULL;
}
static MINIOS_AUDIO_STUB plat_audio_stream_t minios_open_audio_device_stream(plat_audio_format_t format,
														   int channels, int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return NULL;
}
static MINIOS_AUDIO_STUB int minios_bind_audio_stream(plat_audio_device_t device, plat_audio_stream_t stream)
{
	(void)device;
	(void)stream;
	return -1;
}
static MINIOS_AUDIO_STUB void minios_destroy_audio_stream(plat_audio_stream_t stream) { (void)stream; }
static MINIOS_AUDIO_STUB int minios_put_audio_stream_data(plat_audio_stream_t stream, const void *data, int len)
{
	(void)stream;
	(void)data;
	(void)len;
	return -1;
}
static MINIOS_AUDIO_STUB int minios_flush_audio_stream(plat_audio_stream_t stream)
{
	(void)stream;
	return -1;
}
static MINIOS_AUDIO_STUB int minios_get_audio_stream_queued(plat_audio_stream_t stream)
{
	(void)stream;
	return -1;
}
static MINIOS_AUDIO_STUB void minios_clear_audio_stream(plat_audio_stream_t stream) { (void)stream; }
static MINIOS_AUDIO_STUB int minios_set_audio_stream_gain(plat_audio_stream_t stream, float gain)
{
	(void)stream;
	(void)gain;
	return -1;
}
static MINIOS_AUDIO_STUB void minios_resume_audio_stream_device(plat_audio_stream_t stream) { (void)stream; }
static MINIOS_AUDIO_STUB int minios_load_wav(const char *filepath, plat_audio_spec_t *spec, uint8_t **buffer,
						   uint32_t *length)
{
	(void)filepath;
	(void)spec;
	(void)buffer;
	(void)length;
	return -1;
}
static MINIOS_AUDIO_STUB void minios_mem_free(void *ptr)
{
	if (ptr) free(ptr);
}

/* ---- sync ---- */

static plat_mutex_t minios_create_mutex(void)
{
	plat_mutex_t m = malloc(sizeof(struct plat_mutex));
	if (!m) return NULL;
	m->flag = 0;
	return m;
}

static void minios_destroy_mutex(plat_mutex_t mutex)
{
	if (mutex) free(mutex);
}

static void minios_lock_mutex(plat_mutex_t mutex)
{
	if (!mutex) return;
	while (__sync_lock_test_and_set(&mutex->flag, 1)) {}
}

static void minios_unlock_mutex(plat_mutex_t mutex)
{
	if (!mutex) return;
	__sync_lock_release(&mutex->flag);
}

/* ---- io: stdio backed ---- */

static plat_io_t *minios_io_open_read(const char *path)
{
	if (!path) return NULL;
	plat_io_t *io = malloc(sizeof(plat_io_t));
	if (!io) return NULL;
	io->fp = fopen(path, "rb");
	if (!io->fp) {
		free(io);
		return NULL;
	}
	return io;
}

static void minios_io_close(plat_io_t *io)
{
	if (io) {
		if (io->fp) fclose(io->fp);
		free(io);
	}
}

static size_t minios_io_read(plat_io_t *io, void *buf, size_t size)
{
	if (!io || !buf) return 0;
	return fread(buf, 1, size, io->fp);
}

static int64_t minios_io_seek(plat_io_t *io, int64_t offset, int whence)
{
	return (io && io->fp) ? (int64_t)fseek(io->fp, (long)offset, whence) : -1;
}

static int64_t minios_io_tell(plat_io_t *io)
{
	return (io && io->fp) ? (int64_t)ftell(io->fp) : -1;
}

static int64_t minios_io_size(plat_io_t *io)
{
	if (!io || !io->fp) return -1;
	long pos = ftell(io->fp);
	fseek(io->fp, 0, SEEK_END);
	long size = ftell(io->fp);
	fseek(io->fp, pos, SEEK_SET);
	return (int64_t)size;
}

/* ---- logging ---- */

static void minios_log_va(const char *tag, const char *fmt, va_list args)
{
	char buf[512];
	vsnprintf(buf, sizeof(buf), fmt, args);

	if (tag) {
		printf("%s: %s\n", tag, buf);
	} else {
		printf("%s\n", buf);
	}
	fflush(stdout);
}

static void minios_log_debug(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	minios_log_va("[DEBUG]", fmt, args);
	va_end(args);
}

static void minios_log_info(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	minios_log_va(NULL, fmt, args);
	va_end(args);
}

static void minios_log_warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	minios_log_va("[WARN]", fmt, args);
	va_end(args);
}

static void minios_log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	minios_log_va("[ERROR]", fmt, args);
	va_end(args);
}

static void minios_log_critical(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	minios_log_va("[CRITICAL]", fmt, args);
	va_end(args);
}

static const plat_interface_t minios_interface = {
	.core =
		{
			.init		  = minios_init,
			.quit		  = minios_quit,
			.delay		  = minios_delay,
			.get_ticks	  = minios_get_ticks,
			.log_debug	  = minios_log_debug,
			.log_info	  = minios_log_info,
			.log_warn	  = minios_log_warn,
			.log_error	  = minios_log_error,
			.log_critical = minios_log_critical,
		},
	.window =
		{
			.create_window	 = minios_create_window,
			.destroy_window	 = minios_destroy_window,
			.poll_event		 = minios_poll_event,
			.get_mouse_state = minios_get_mouse_state,
		},
	.renderer =
		{
			.create_renderer			= minios_create_renderer,
			.destroy_renderer			= minios_destroy_renderer,
			.set_render_draw_color		= minios_set_render_draw_color,
			.set_render_draw_blend_mode = minios_set_render_draw_blend_mode,
			.render_clear				= minios_render_clear,
			.render_present				= minios_render_present,
			.render_point				= minios_render_point,
			.render_line				= minios_render_line,
			.render_rect				= minios_render_rect,
			.render_fill_rect			= minios_render_fill_rect,
			.render_texture				= minios_render_texture,
		},
	.texture =
		{
			.create_texture_from_surface = minios_create_texture_from_surface,
			.create_texture				 = minios_create_texture,
			.update_texture				 = minios_update_texture,
			.get_texture_size			 = minios_get_texture_size,
			.destroy_texture			 = minios_destroy_texture,
			.load_image					 = minios_load_image,
			.destroy_surface			 = minios_destroy_surface,
		},
	.text =
		{
			.init_ttf			 = minios_init_ttf,
			.quit_ttf			 = minios_quit_ttf,
			.open_font			 = minios_open_font,
			.close_font			 = minios_close_font,
			.render_text_blended = minios_render_text_blended,
			.get_string_size	 = minios_get_string_size,
		},
	.audio = {},
	.sync =
		{
			.create_mutex  = minios_create_mutex,
			.destroy_mutex = minios_destroy_mutex,
			.lock_mutex	   = minios_lock_mutex,
			.unlock_mutex  = minios_unlock_mutex,
		},
	.io =
		{
			.open_read = minios_io_open_read,
			.close	   = minios_io_close,
			.read	   = minios_io_read,
			.seek	   = minios_io_seek,
			.tell	   = minios_io_tell,
			.size	   = minios_io_size,
		},
	.capabilities = 0,
};

const plat_interface_t *plat_minios_interface(void)
{
	return &minios_interface;
}

#endif /* USE_BACKEND_MINIOS */