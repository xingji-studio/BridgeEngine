#include "internal/platform/platform.h"
#include "internal/platform/platform_types.h"

#ifdef __XJ380_OS__

#include <krlibc.h>
#include <xapi.h>
#include <xposix/stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline UINT32 make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (UINT32)r << 24 | (UINT32)g << 16 | (UINT32)b << 8 | (UINT32)a;
}

/* Casting a negative float to UINT32 wraps to a huge value; clip instead so
 * off-screen coordinates stay clipped instead of drawing at ~4 billion. */
static UINT32 xj380_clip_coord(float value)
{
	return (value > 0.0f) ? (UINT32)value : 0u;
}

static char *xj380_copy_string(const char *text)
{
	if (!text) return NULL;
	size_t len	= strlen(text) + 1;
	char  *copy = (char *)malloc(len);
	if (copy) {
		memcpy(copy, text, len);
	}
	return copy;
}

static char *xj380_copy_string_len(const char *text, int len)
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

static WSTR xj380_xapi_string(const char *text)
{
	/*
	 * XJ380 user headers currently define WSTR as char*, not as a wide string.
	 * Keep the const removal localized so call sites do not imply UTF-16 or
	 * wchar_t conversion.
	 */
	return const_cast<char *>(text ? text : "");
}

struct plat_window {
	HDLE handle;
	int	 width;
	int	 height;
};

struct plat_renderer {
	plat_window_t	  window;
	uint8_t			  r, g, b, a;
	plat_blend_mode_t blend_mode;
};

enum xj380_tex_type { TEX_PIXELS, TEX_IMAGE, TEX_TEXT };

struct plat_texture {
	enum xj380_tex_type type;
	int					width;
	int					height;
	union {
		struct {
			uint8_t *pixels;
			int		 pitch;
		} pixel;
		struct {
			char *filepath;
		} image;
		struct {
			char   *text;
			float	size;
			uint8_t r, g, b, a;
		} text;
	} data;
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

enum xj380_surf_type { SURF_PIXELS, SURF_IMAGE, SURF_TEXT };

struct plat_surface {
	enum xj380_surf_type type;
	int					 width;
	int					 height;
	union {
		struct {
			uint8_t *pixels;
			int		 pitch;
		} pixel;
		struct {
			char *filepath;
		} image;
		struct {
			char   *text;
			float	size;
			uint8_t r, g, b, a;
		} text;
	} data;
};

#define XJ380_EVENT_QUEUE_SIZE 256

static plat_event_t g_event_queue[XJ380_EVENT_QUEUE_SIZE];
static volatile int g_event_head = 0;
static volatile int g_event_tail = 0;

static plat_window_t g_current_window = NULL;

static float g_mouse_x = 0.0f;
static float g_mouse_y = 0.0f;

static UINT64 g_start_time = 0;

static uint32_t convert_xj380_spkey(UINT64 xj_key)
{
	switch (xj_key) {
	case 128:
		return PLAT_KEY_ESC;
	case 8:
		return PLAT_KEY_BACKSPACE;
	case 130:
		return PLAT_KEY_TAB;
	case 10:
		return PLAT_KEY_ENTER;
	case 132:
		return PLAT_KEY_CAPS;
	case 133:
		return PLAT_KEY_SHIFT;
	case 134:
		return PLAT_KEY_CTRL;
	case 135:
		return PLAT_KEY_ALT;
	case 136:
		return PLAT_KEY_F1;
	case 137:
		return PLAT_KEY_F2;
	case 138:
		return PLAT_KEY_F3;
	case 139:
		return PLAT_KEY_F4;
	case 140:
		return PLAT_KEY_F5;
	case 141:
		return PLAT_KEY_F6;
	case 142:
		return PLAT_KEY_F7;
	case 143:
		return PLAT_KEY_F8;
	case 144:
		return PLAT_KEY_F9;
	case 145:
		return PLAT_KEY_F10;
	case 146:
		return PLAT_KEY_F11;
	case 147:
		return PLAT_KEY_F12;
	case 149:
		return PLAT_KEY_NUML;
	case 150:
		return PLAT_KEY_SCROLL;
	default:
		return (uint32_t)xj_key;
	}
}

static void xj380_msg_handler(UINT64 Type, UINT64 hData, UINT64 lData)
{
	plat_event_t event;
	memset(&event, 0, sizeof(event));

	switch (Type) {
	case MSG_CHAR: {

		event.type = PLAT_EVENT_KEY_DOWN;
		uint8_t ch = (uint8_t)(hData & 0xFF);
		if (ch >= 32 && ch < 128) {

			event.data.key.key = ch;
		} else {

			event.data.key.key = (uint32_t)hData;
		}
		break;
	}
	case MSG_SPCHAR: {

		event.type		   = PLAT_EVENT_KEY_DOWN;
		event.data.key.key = convert_xj380_spkey(hData);
		break;
	}
	case MSG_LBUTTON: {
		event.type				 = PLAT_EVENT_MOUSE_BUTTON_DOWN;
		event.data.button.x		 = (float)hData;
		event.data.button.y		 = (float)lData;
		event.data.button.button = PLAT_BUTTON_LEFT;
		break;
	}
	case MSG_RBUTTON: {
		event.type				 = PLAT_EVENT_MOUSE_BUTTON_DOWN;
		event.data.button.x		 = (float)hData;
		event.data.button.y		 = (float)lData;
		event.data.button.button = PLAT_BUTTON_RIGHT;
		break;
	}
	case MSG_MBUTTON: {
		event.type				 = PLAT_EVENT_MOUSE_BUTTON_DOWN;
		event.data.button.x		 = (float)hData;
		event.data.button.y		 = (float)lData;
		event.data.button.button = 2;
		break;
	}
	case MSG_MOVE: {
		event.type			= PLAT_EVENT_MOUSE_MOTION;
		event.data.motion.x = (float)hData;
		event.data.motion.y = (float)lData;

		g_mouse_x = (float)hData;
		g_mouse_y = (float)lData;
		break;
	}
	default:

		return;
	}

	int next_tail = (g_event_tail + 1) % XJ380_EVENT_QUEUE_SIZE;
	if (next_tail != g_event_head) {

		g_event_queue[g_event_tail] = event;
		g_event_tail				= next_tail;
	}
}

static int xj380_init(uint32_t flags)
{

	g_start_time = xapi_GetTime();
	(void)flags;
	return 0;
}

static void xj380_quit(void) {}

static plat_window_t xj380_create_window(const char *title, int width, int height)
{
	plat_window_t w = (plat_window_t)malloc(sizeof(struct plat_window));
	if (!w) return NULL;

	XWINDOW xwin;
	memset(&xwin, 0, sizeof(xwin));
	xwin.width	= (UINT32)width;
	xwin.height = (UINT32)height;
	xwin.title	= xj380_xapi_string(title);
	xwin.sets	= XWIN_NORMAL;

	xapi_CreateWindow(&w->handle, &xwin);
	if (!w->handle) {
		free(w);
		return NULL;
	}

	w->width		 = width;
	w->height		 = height;
	g_current_window = w;

	SetMsgPrcor(w->handle, xj380_msg_handler);

	return w;
}

static void xj380_destroy_window(plat_window_t window)
{
	if (window) {
		if (window->handle) {
			xapi_CloseWindow(window->handle);
		}
		if (g_current_window == window) {
			g_current_window = NULL;
		}
		free(window);
	}
}

static void *xj380_get_native_window(plat_window_t window)
{
	(void)window;
	return NULL;
}

static plat_renderer_t xj380_create_renderer(plat_window_t window)
{
	if (!window) return NULL;

	plat_renderer_t r = (plat_renderer_t)malloc(sizeof(struct plat_renderer));
	if (!r) return NULL;

	r->window	  = window;
	r->r		  = 0;
	r->g		  = 0;
	r->b		  = 0;
	r->a		  = 255;
	r->blend_mode = PLAT_BLENDMODE_BLEND;
	return r;
}

static void xj380_destroy_renderer(plat_renderer_t renderer)
{
	if (renderer) {
		free(renderer);
	}
}

static void *xj380_get_native_renderer(plat_renderer_t renderer)
{
	(void)renderer;
	return NULL;
}

static void xj380_set_render_draw_color(plat_renderer_t renderer, uint8_t r, uint8_t g, uint8_t b,
										uint8_t a)
{
	if (renderer) {
		renderer->r = r;
		renderer->g = g;
		renderer->b = b;
		renderer->a = a;
	}
}

static void xj380_set_render_draw_blend_mode(plat_renderer_t renderer, plat_blend_mode_t mode)
{
	if (renderer) {
		renderer->blend_mode = mode;
	}
}

static void xj380_render_clear(plat_renderer_t renderer)
{
	if (!renderer || !renderer->window) return;

	HDLE handle = renderer->window->handle;
	int	 w		= renderer->window->width;
	int	 h		= renderer->window->height;

	if (w <= 0 || h <= 0) return;

	/* Compute in 64-bit first: w * h * sizeof(XCOLORA) overflows 32-bit int
	 * for large windows and would allocate a short buffer. */
	int64_t pixel_count = (int64_t)w * h;
	if (pixel_count > (int64_t)0x7FFFFFFF / (int64_t)sizeof(XCOLORA)) return;

	XCOLORA *buf = (XCOLORA *)xapi_AllocateMemory((UINT64)pixel_count * sizeof(XCOLORA));
	if (!buf) return;

	XCOLORA fill_color;
	fill_color.Red	 = renderer->r;
	fill_color.Green = renderer->g;
	fill_color.Blue	 = renderer->b;
	fill_color.Alpha = renderer->a;

	for (int64_t i = 0; i < pixel_count; i++) {
		buf[i] = fill_color;
	}

	xapi_WriteBufferA(handle, 0, 0, (UINT32)w, (UINT32)h, buf);
	xapi_FreeMemory(buf);
}

static void xj380_render_present(plat_renderer_t renderer)
{
	if (!renderer || !renderer->window) return;
	xapi_RefreshWindow(renderer->window->handle);
}

static void xj380_render_point(plat_renderer_t renderer, float x, float y)
{
	if (!renderer || !renderer->window) return;
	UINT32 color = make_rgba(renderer->r, renderer->g, renderer->b, renderer->a);
	xapi_DrawPoint(renderer->window->handle, xj380_clip_coord(x), xj380_clip_coord(y), color);
}

static void xj380_render_line(plat_renderer_t renderer, float x1, float y1, float x2, float y2)
{
	if (!renderer || !renderer->window) return;
	UINT32 color = make_rgba(renderer->r, renderer->g, renderer->b, renderer->a);
	xapi_DrawLine(renderer->window->handle, xj380_clip_coord(x1), xj380_clip_coord(y1),
				  xj380_clip_coord(x2), xj380_clip_coord(y2), color);
}

static void xj380_render_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	if (!renderer || !renderer->window) return;
	UINT32 color = make_rgba(renderer->r, renderer->g, renderer->b, renderer->a);
	xapi_DrawRect(renderer->window->handle, xj380_clip_coord(x), xj380_clip_coord(y),
				  xj380_clip_coord(x + w), xj380_clip_coord(y + h), color, false);
}

static void xj380_render_fill_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	if (!renderer || !renderer->window) return;
	UINT32 color = make_rgba(renderer->r, renderer->g, renderer->b, renderer->a);
	xapi_DrawRect(renderer->window->handle, xj380_clip_coord(x), xj380_clip_coord(y),
				  xj380_clip_coord(x + w), xj380_clip_coord(y + h), color, true);
}

static void xj380_render_texture(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
								 float w, float h)
{
	if (!renderer || !texture || !renderer->window) return;

	HDLE handle = renderer->window->handle;

	switch (texture->type) {
	case TEX_IMAGE:

		if (texture->data.image.filepath) {
			xapi_DrawPicture(handle, xj380_clip_coord(x), xj380_clip_coord(y),
							 xj380_clip_coord(w), xj380_clip_coord(h),
							 xj380_xapi_string(texture->data.image.filepath));
		}
		break;

	case TEX_TEXT:

		if (texture->data.text.text) {
			UINT32 color = make_rgba(texture->data.text.r, texture->data.text.g,
									 texture->data.text.b, texture->data.text.a);
			xapi_DrawText(handle, xj380_clip_coord(x), xj380_clip_coord(y),
						  xj380_xapi_string(texture->data.text.text),
						  (UINT32)texture->data.text.size, color);
		}
		break;

	case TEX_PIXELS:

		if (texture->data.pixel.pixels && texture->width > 0 && texture->height > 0) {

			int render_w = (w > 0) ? (int)w : texture->width;
			int render_h = (h > 0) ? (int)h : texture->height;

			if (render_w == texture->width && render_h == texture->height) {

				xapi_WriteBufferA(handle, xj380_clip_coord(x), xj380_clip_coord(y),
								  (UINT32)render_w, (UINT32)render_h,
								  (XCOLORA *)texture->data.pixel.pixels);
			} else {

				int64_t scaled_count = (int64_t)render_w * render_h;
				if (scaled_count <= 0 ||
					scaled_count > (int64_t)0x7FFFFFFF / (int64_t)sizeof(XCOLORA))
					return;
				XCOLORA *scaled = (XCOLORA *)xapi_AllocateMemory((UINT64)scaled_count *
																 sizeof(XCOLORA));
				if (scaled) {
					XCOLORA *src = (XCOLORA *)texture->data.pixel.pixels;
					for (int sy = 0; sy < render_h; sy++) {
						int src_y = sy * texture->height / render_h;
						for (int sx = 0; sx < render_w; sx++) {
							int src_x				   = sx * texture->width / render_w;
							scaled[sy * render_w + sx] = src[src_y * texture->width + src_x];
						}
					}
					xapi_WriteBufferA(handle, xj380_clip_coord(x), xj380_clip_coord(y),
									  (UINT32)render_w, (UINT32)render_h, scaled);
					xapi_FreeMemory(scaled);
				}
			}
		}
		break;
	}
}

static plat_texture_t xj380_create_texture_from_surface(plat_renderer_t renderer,
														plat_surface_t *surface)
{
	if (!renderer || !surface) return NULL;

	plat_texture_t t = (plat_texture_t)malloc(sizeof(struct plat_texture));
	if (!t) return NULL;

	memset(t, 0, sizeof(struct plat_texture));

	switch (surface->type) {
	case SURF_IMAGE:
		t->type	  = TEX_IMAGE;
		t->width  = surface->width;
		t->height = surface->height;
		if (surface->data.image.filepath) {
			t->data.image.filepath = xj380_copy_string(surface->data.image.filepath);
		}
		break;

	case SURF_TEXT:
		t->type	  = TEX_TEXT;
		t->width  = surface->width;
		t->height = surface->height;
		if (surface->data.text.text) {
			t->data.text.text = xj380_copy_string(surface->data.text.text);
		}
		t->data.text.size = surface->data.text.size;
		t->data.text.r	  = surface->data.text.r;
		t->data.text.g	  = surface->data.text.g;
		t->data.text.b	  = surface->data.text.b;
		t->data.text.a	  = surface->data.text.a;
		break;

	case SURF_PIXELS: {
		t->type				= TEX_PIXELS;
		t->width			= surface->width;
		t->height			= surface->height;
		t->data.pixel.pitch = surface->width * 4;
		int64_t size		= (int64_t)surface->width * surface->height * 4;
		if (size <= 0 || size > (int64_t)0x7FFFFFFF) {
			free(t);
			return NULL;
		}
		t->data.pixel.pixels = (uint8_t *)xapi_AllocateMemory((UINT64)size);
		if (!t->data.pixel.pixels || !surface->data.pixel.pixels) break;
		/* Copy row by row: the source surface may carry padding (pitch
		 * larger than width*4); a flat memcpy would misalign or overread. */
		size_t row_bytes = (size_t)surface->width * 4;
		for (int y = 0; y < surface->height; y++) {
			memcpy(t->data.pixel.pixels + (size_t)y * row_bytes,
				   surface->data.pixel.pixels + (size_t)y * (size_t)surface->data.pixel.pitch,
				   row_bytes);
		}
		break;
	}
	}

	return t;
}

static plat_texture_t xj380_create_texture(plat_renderer_t renderer, plat_pixel_format_t format,
										   plat_texture_access_t access, int width, int height)
{
	if (!renderer) return NULL;
	if (format != PLAT_PIXELFORMAT_ARGB8888) return NULL;

	plat_texture_t t = (plat_texture_t)malloc(sizeof(struct plat_texture));
	if (!t) return NULL;

	t->type				= TEX_PIXELS;
	t->width			= width;
	t->height			= height;
	t->data.pixel.pitch = width * 4;

	/* Compute in 64-bit first: width * height * 4 overflows 32-bit int for
	 * large textures and would allocate a short buffer. */
	int64_t size = (int64_t)width * height * 4;
	if (size <= 0 || size > (int64_t)0x7FFFFFFF) {
		free(t);
		return NULL;
	}
	t->data.pixel.pixels = (uint8_t *)xapi_AllocateMemory((UINT64)size);
	if (!t->data.pixel.pixels) {
		free(t);
		return NULL;
	}
	memset(t->data.pixel.pixels, 0, (size_t)size);

	(void)format;
	(void)access;
	return t;
}

static int xj380_update_texture(plat_texture_t texture, const void *pixels, int pitch)
{
	if (!texture || !pixels || texture->type != TEX_PIXELS) return -1;

	XCOLORA		  *dst = (XCOLORA *)texture->data.pixel.pixels;
	const uint8_t *src = (const uint8_t *)pixels;

	for (int y = 0; y < texture->height; y++) {
		for (int x = 0; x < texture->width; x++) {
			int src_idx = y * pitch + x * 4;
			int dst_idx = y * texture->width + x;

			dst[dst_idx].Alpha = src[src_idx + 0];
			dst[dst_idx].Red   = src[src_idx + 1];
			dst[dst_idx].Green = src[src_idx + 2];
			dst[dst_idx].Blue  = src[src_idx + 3];
		}
	}

	return 0;
}

static int xj380_get_texture_size(plat_texture_t texture, int *width, int *height)
{
	if (!texture || !width || !height) return -1;
	*width	= texture->width;
	*height = texture->height;
	return 0;
}

static void xj380_destroy_texture(plat_texture_t texture)
{
	if (!texture) return;

	switch (texture->type) {
	case TEX_PIXELS:
		if (texture->data.pixel.pixels) {
			xapi_FreeMemory(texture->data.pixel.pixels);
		}
		break;
	case TEX_IMAGE:
		if (texture->data.image.filepath) {
			free(texture->data.image.filepath);
		}
		break;
	case TEX_TEXT:
		if (texture->data.text.text) {
			free(texture->data.text.text);
		}
		break;
	}

	free(texture);
}

static plat_surface_t *xj380_load_image(const char *filepath)
{
	if (!filepath) return NULL;

	plat_surface_t *s = (plat_surface_t *)malloc(sizeof(plat_surface_t));
	if (!s) return NULL;

	memset(s, 0, sizeof(plat_surface_t));

	UINT32 w = 0, h = 0;
	xapi_GetPicSize(&w, &h, xj380_xapi_string(filepath));

	/* Treat a 0x0 result as a failed/missing image instead of handing the
	 * caller a "successful" surface that can never draw anything. */
	if (w == 0 || h == 0) {
		free(s);
		return NULL;
	}

	s->type				   = SURF_IMAGE;
	s->width			   = (int)w;
	s->height			   = (int)h;
	s->data.image.filepath = xj380_copy_string(filepath);

	if (!s->data.image.filepath) {
		free(s);
		return NULL;
	}

	return s;
}

static void xj380_destroy_surface(plat_surface_t *surface)
{
	if (!surface) return;

	switch (surface->type) {
	case SURF_PIXELS:
		if (surface->data.pixel.pixels) {
			xapi_FreeMemory(surface->data.pixel.pixels);
		}
		break;
	case SURF_IMAGE:
		if (surface->data.image.filepath) {
			free(surface->data.image.filepath);
		}
		break;
	case SURF_TEXT:
		if (surface->data.text.text) {
			free(surface->data.text.text);
		}
		break;
	}

	free(surface);
}

static int xj380_poll_event(plat_event_t *event)
{
	if (!event) return 0;

	if (g_event_head == g_event_tail) {

		return 0;
	}

	*event		 = g_event_queue[g_event_head];
	g_event_head = (g_event_head + 1) % XJ380_EVENT_QUEUE_SIZE;
	return 1;
}

static void xj380_get_mouse_state(float *x, float *y)
{
	if (x) *x = g_mouse_x;
	if (y) *y = g_mouse_y;
}

static int xj380_init_ttf(void)
{

	return 0;
}

static void xj380_quit_ttf(void) {}

static plat_font_t xj380_open_font(const char *filepath, float size)
{
	(void)filepath;

	plat_font_t f = (plat_font_t)malloc(sizeof(struct plat_font));
	if (!f) return NULL;

	f->size = size;
	return f;
}

static void xj380_close_font(plat_font_t font)
{
	if (font) {
		free(font);
	}
}

static plat_surface_t *xj380_render_text_blended(plat_font_t font, const char *text, int len,
												 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (!font || !text) return NULL;

	plat_surface_t *s = (plat_surface_t *)malloc(sizeof(plat_surface_t));
	if (!s) return NULL;

	memset(s, 0, sizeof(plat_surface_t));

	s->type = SURF_TEXT;

	s->data.text.text = xj380_copy_string_len(text, len);
	if (!s->data.text.text) {
		free(s);
		return NULL;
	}

	UINT64 text_w = xapi_CalcTextWidth(xj380_xapi_string(s->data.text.text), (UINT32)font->size);
	s->width	  = (int)text_w;
	s->height	  = (int)(font->size * 1.2f);

	s->data.text.size = font->size;
	s->data.text.r	  = r;
	s->data.text.g	  = g;
	s->data.text.b	  = b;
	s->data.text.a	  = a;

	return s;
}

static int xj380_get_string_size(plat_font_t font, const char *text, int len, int *w, int *h)
{
	if (!font || !text) return -1;

	char *text_copy = xj380_copy_string_len(text, len);
	if (!text_copy) return -1;

	UINT64 text_w = xapi_CalcTextWidth(xj380_xapi_string(text_copy), (UINT32)font->size);
	if (w) *w = (int)text_w;
	if (h) *h = (int)(font->size * 1.2f);

	free(text_copy);
	return 0;
}

static plat_audio_device_t xj380_open_audio_device(plat_audio_format_t format, int channels,
												   int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return NULL;
}

static void xj380_close_audio_device(plat_audio_device_t device)
{
	(void)device;
}

static plat_audio_stream_t xj380_create_audio_stream(const plat_audio_spec_t *spec)
{
	(void)spec;
	return NULL;
}

static plat_audio_stream_t xj380_open_audio_device_stream(plat_audio_format_t format, int channels,
														  int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return NULL;
}

static int xj380_bind_audio_stream(plat_audio_device_t device, plat_audio_stream_t stream)
{
	(void)device;
	(void)stream;
	return -1;
}

static void xj380_destroy_audio_stream(plat_audio_stream_t stream)
{
	(void)stream;
}

static int xj380_put_audio_stream_data(plat_audio_stream_t stream, const void *data, int len)
{
	(void)stream;
	(void)data;
	(void)len;
	return -1;
}

static int xj380_flush_audio_stream(plat_audio_stream_t stream)
{
	(void)stream;
	return -1;
}

static int xj380_get_audio_stream_queued(plat_audio_stream_t stream)
{
	(void)stream;
	return -1;
}

static void xj380_clear_audio_stream(plat_audio_stream_t stream)
{
	(void)stream;
}

static int xj380_set_audio_stream_gain(plat_audio_stream_t stream, float gain)
{
	(void)stream;
	(void)gain;
	return -1;
}

static void xj380_resume_audio_stream_device(plat_audio_stream_t stream)
{
	(void)stream;
}

static int xj380_load_wav(const char *filepath, plat_audio_spec_t *spec, uint8_t **buffer,
						  uint32_t *length)
{
	(void)filepath;
	(void)spec;
	(void)buffer;
	(void)length;
	return -1;
}

static void xj380_mem_free(void *ptr)
{
	if (ptr) {
		xapi_FreeMemory(ptr);
	}
}

static plat_io_t *xj380_io_open_read(const char *path)
{
	if (!path) return NULL;
	plat_io_t *io = (plat_io_t *)malloc(sizeof(plat_io_t));
	if (!io) return NULL;
	io->fp = fopen(path, "rb");
	if (!io->fp) {
		free(io);
		return NULL;
	}
	return io;
}

static void xj380_io_close(plat_io_t *io)
{
	if (io) {
		if (io->fp) fclose(io->fp);
		free(io);
	}
}

static size_t xj380_io_read(plat_io_t *io, void *buf, size_t size)
{
	if (!io || !io->fp || !buf) return 0;
	return fread(buf, 1, size, io->fp);
}

static int64_t xj380_io_seek(plat_io_t *io, int64_t offset, int whence)
{
	if (!io || !io->fp) return -1;
	/* Return the new position like the SDL3 backend (SDL_SeekIO) does;
	 * fseek returns a status code, which broke the cross-backend contract. */
	if (fseek(io->fp, (long)offset, whence) != 0) return -1;
	return (int64_t)ftell(io->fp);
}

static int64_t xj380_io_tell(plat_io_t *io)
{
	if (!io || !io->fp) return -1;
	return ftell(io->fp);
}

static int64_t xj380_io_size(plat_io_t *io)
{
	if (!io || !io->fp) return -1;
	long pos = ftell(io->fp);
	fseek(io->fp, 0, SEEK_END);
	long size = ftell(io->fp);
	fseek(io->fp, pos, SEEK_SET);
	return size;
}

static plat_mutex_t xj380_create_mutex(void)
{
	plat_mutex_t m = (plat_mutex_t)malloc(sizeof(struct plat_mutex));
	if (!m) return NULL;
	m->flag = 0;
	return m;
}

static void xj380_destroy_mutex(plat_mutex_t mutex)
{
	if (mutex) free(mutex);
}

static void xj380_lock_mutex(plat_mutex_t mutex)
{
	if (!mutex) return;
	while (__sync_lock_test_and_set(&mutex->flag, 1)) {}
}

static void xj380_unlock_mutex(plat_mutex_t mutex)
{
	if (!mutex) return;
	__sync_lock_release(&mutex->flag);
}

static void xj380_delay(uint32_t ms)
{
	xapi_Sleep((UINT64)ms);
}

static uint32_t xj380_get_ticks(void)
{

	UINT64 current = xapi_GetTime();
	UINT64 elapsed = current - g_start_time;
	return (uint32_t)(elapsed * 1000);
}

static void xj380_log_debug(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	{
		char buf[512];
		vsnprintf(buf, sizeof(buf), fmt, args);
		xapi_Output(xj380_xapi_string("[DEBUG] "));
		xapi_Output(xj380_xapi_string(buf));
	}
	va_end(args);
}

static void xj380_log_info(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	{
		char buf[512];
		vsnprintf(buf, sizeof(buf), fmt, args);
		xapi_Output(xj380_xapi_string(buf));
	}
	va_end(args);
}

static void xj380_log_warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	{
		char buf[512];
		vsnprintf(buf, sizeof(buf), fmt, args);
		xapi_Output(xj380_xapi_string("[WARN] "));
		xapi_Output(xj380_xapi_string(buf));
	}
	va_end(args);
}

static void xj380_log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	{
		char buf[512];
		vsnprintf(buf, sizeof(buf), fmt, args);
		xapi_Output(xj380_xapi_string("[ERROR] "));
		xapi_Output(xj380_xapi_string(buf));
	}
	va_end(args);
}

static void xj380_log_critical(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	{
		char buf[512];
		vsnprintf(buf, sizeof(buf), fmt, args);
		xapi_Output(xj380_xapi_string("[CRITICAL] "));
		xapi_Output(xj380_xapi_string(buf));
	}
	va_end(args);
}

static const plat_interface_t xj380_interface = {
	.core =
		{
			.init		  = xj380_init,
			.quit		  = xj380_quit,
			.delay		  = xj380_delay,
			.get_ticks	  = xj380_get_ticks,
			.log_debug	  = xj380_log_debug,
			.log_info	  = xj380_log_info,
			.log_warn	  = xj380_log_warn,
			.log_error	  = xj380_log_error,
			.log_critical = xj380_log_critical,
		},
	.window =
		{
			.create_window	 = xj380_create_window,
			.destroy_window	 = xj380_destroy_window,
			.poll_event		 = xj380_poll_event,
			.get_mouse_state = xj380_get_mouse_state,
			.get_native_window = xj380_get_native_window,
		},
	.renderer =
		{
			.create_renderer			= xj380_create_renderer,
			.destroy_renderer			= xj380_destroy_renderer,
			.set_render_draw_color		= xj380_set_render_draw_color,
			.set_render_draw_blend_mode = xj380_set_render_draw_blend_mode,
			.render_clear				= xj380_render_clear,
			.render_present				= xj380_render_present,
			.render_point				= xj380_render_point,
			.render_line				= xj380_render_line,
			.render_rect				= xj380_render_rect,
			.render_fill_rect			= xj380_render_fill_rect,
			.render_texture				= xj380_render_texture,
			.get_native_renderer		= xj380_get_native_renderer,
		},
	.texture =
		{
			.create_texture_from_surface = xj380_create_texture_from_surface,
			.create_texture				 = xj380_create_texture,
			.update_texture				 = xj380_update_texture,
			.get_texture_size			 = xj380_get_texture_size,
			.destroy_texture			 = xj380_destroy_texture,
			.load_image					 = xj380_load_image,
			.destroy_surface			 = xj380_destroy_surface,
		},
	.text =
		{
			.init_ttf			 = xj380_init_ttf,
			.quit_ttf			 = xj380_quit_ttf,
			.open_font			 = xj380_open_font,
			.close_font			 = xj380_close_font,
			.render_text_blended = xj380_render_text_blended,
			.get_string_size	 = xj380_get_string_size,
		},
	.audio = {},
	.sync =
		{
			.create_mutex  = xj380_create_mutex,
			.destroy_mutex = xj380_destroy_mutex,
			.lock_mutex	   = xj380_lock_mutex,
			.unlock_mutex  = xj380_unlock_mutex,
		},
	.io =
		{
			.open_read = xj380_io_open_read,
			.close	   = xj380_io_close,
			.read	   = xj380_io_read,
			.seek	   = xj380_io_seek,
			.tell	   = xj380_io_tell,
			.size	   = xj380_io_size,
		},
	.capabilities = 0,
};

const plat_interface_t *plat_xj380_interface(void)
{
	return &xj380_interface;
}

#ifdef __cplusplus
}
#endif

#endif
