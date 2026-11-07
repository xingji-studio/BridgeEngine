#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/frame.h"
#include "internal/engine/math.h"
#include "internal/engine/render_context.h"
#include "internal/engine/texture_internal.h"
#include "internal/engine/transform.h"
#include "internal/platform/platform.h"
#include <math.h>
#include <stdlib.h>

static int g_vsync;
static int g_fullscreen;
static int g_logical_width;
static int g_logical_height;
static int g_window_width;
static int g_window_height;

static void bapi_event_from_platform(bapi_event_t *dst, const plat_event_t *src)
{
	switch (src->type) {
	case PLAT_EVENT_QUIT:
		dst->type = BAPI_EVENT_QUIT;
		break;
	case PLAT_EVENT_KEY_DOWN:
		dst->type		  = BAPI_EVENT_KEY_DOWN;
		dst->data.key.key = src->data.key.key;
		break;
	case PLAT_EVENT_KEY_UP:
		dst->type		  = BAPI_EVENT_KEY_UP;
		dst->data.key.key = src->data.key.key;
		break;
	case PLAT_EVENT_MOUSE_BUTTON_DOWN:
		dst->type				= BAPI_EVENT_MOUSE_BUTTON_DOWN;
		dst->data.button.x		= src->data.button.x;
		dst->data.button.y		= src->data.button.y;
		dst->data.button.button = src->data.button.button;
		break;
	case PLAT_EVENT_MOUSE_BUTTON_UP:
		dst->type				= BAPI_EVENT_MOUSE_BUTTON_UP;
		dst->data.button.x		= src->data.button.x;
		dst->data.button.y		= src->data.button.y;
		dst->data.button.button = src->data.button.button;
		break;
	case PLAT_EVENT_MOUSE_MOTION:
		dst->type		   = BAPI_EVENT_MOUSE_MOTION;
		dst->data.motion.x = src->data.motion.x;
		dst->data.motion.y = src->data.motion.y;
		break;
	case PLAT_EVENT_MOUSE_WHEEL:
		dst->type				= BAPI_EVENT_MOUSE_WHEEL;
		dst->data.wheel.x		= src->data.wheel.x;
		dst->data.wheel.y		= src->data.wheel.y;
		dst->data.wheel.mouse_x = src->data.wheel.mouse_x;
		dst->data.wheel.mouse_y = src->data.wheel.mouse_y;
		break;
	case PLAT_EVENT_WINDOW_RESIZED:
		dst->type				= BAPI_EVENT_WINDOW_RESIZED;
		dst->data.window.width	= src->data.window.width;
		dst->data.window.height = src->data.window.height;
		break;
	default:
		dst->type = BAPI_EVENT_UNKNOWN;
		break;
	}
}

static bapi_vec2_t make_vec2(float x, float y)
{
	bapi_vec2_t v = {x, y};
	return v;
}

static float bapi_sine(float angle)
{
#ifdef __XJ380_OS__
	const float pi		= (float)BAPI_PI;
	const float half_pi = pi / 2.0f;
	const float two_pi	= pi * 2.0f;
	while (angle > pi) angle -= two_pi;
	while (angle < -pi) angle += two_pi;
	if (angle > half_pi) angle = pi - angle;
	if (angle < -half_pi) angle = -pi - angle;

	float angle_squared = angle * angle;
	return angle * (1.0f - angle_squared / 6.0f + angle_squared * angle_squared / 120.0f -
					angle_squared * angle_squared * angle_squared / 5040.0f +
					angle_squared * angle_squared * angle_squared * angle_squared / 362880.0f -
					angle_squared * angle_squared * angle_squared * angle_squared * angle_squared /
						39916800.0f);
#else
	return sinf(angle);
#endif
}

static float bapi_cosine(float angle)
{
#ifdef __XJ380_OS__
	return bapi_sine(angle + (float)BAPI_PI / 2.0f);
#else
	return cosf(angle);
#endif
}

plat_renderer_t bapi_internal_get_renderer(void)
{
	return bapi_runtime_renderer();
}

void *bapi_internal_get_native_window(void)
{
	const plat_interface_t *plat   = plat_get();
	plat_window_t			window = bapi_runtime_window();
	if (!bapi_runtime_is_initialized() || plat == NULL || window == NULL ||
		plat->window.get_native_window == NULL)
		return NULL;
	return plat->window.get_native_window(window);
}

void *bapi_internal_get_native_renderer(void)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_runtime_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL ||
		plat->renderer.get_native_renderer == NULL)
		return NULL;
	return plat->renderer.get_native_renderer(renderer);
}

bapi_window_t bapi_engine_get_window(void)
{
	return bapi_runtime_is_initialized() ? (bapi_window_t)bapi_runtime_window() : NULL;
}

bapi_renderer_t bapi_engine_get_renderer(void)
{
	return bapi_runtime_is_initialized() ? (bapi_renderer_t)bapi_runtime_renderer() : NULL;
}

bapi_renderer_backend_t bapi_engine_get_renderer_backend(void)
{
	const plat_interface_t *platform = plat_get();
	if (platform != NULL && platform->get_renderer_backend != NULL) {
		int backend = platform->get_renderer_backend();
		if (backend == 2) return BAPI_RENDERER_VULKAN;
		if (backend == 1) return BAPI_RENDERER_SDL;
	}
	return BAPI_RENDERER_SDL;
}

int bapi_engine_init(const char *title, int width, int height)
{
	return bapi_engine_init_ex(title, width, height, BAPI_RENDERER_AUTO);
}

int bapi_engine_init_ex(const char *title, int width, int height, bapi_renderer_backend_t backend)
{
	if (bapi_runtime_is_initialized()) return 0;

#if defined(USE_BACKEND_MINIOS)
	const plat_interface_t *platform = plat_minios_interface();
#elif defined(USE_BACKEND_XJ380)
	const plat_interface_t *platform = plat_xj380_interface();
#else
	const plat_interface_t *platform = plat_sdl3_interface();
#endif
	if (platform == NULL) {
		bapi_set_last_error("platform interface is unavailable");
		return 1;
	}
	if (platform->set_renderer_backend != NULL) {
		platform->set_renderer_backend((int)backend);
	}

	int result = bapi_runtime_start(platform, title, width, height);
	if (result != 0) {
		if (backend == BAPI_RENDERER_VULKAN) {
			bapi_set_last_error("Vulkan renderer backend is unavailable");
		} else {
			bapi_set_last_error("engine initialization failed");
		}
		return result;
	}

	bapi_frame_reset();
	bapi_reset_transform();
	bapi_clear_last_error();
	g_vsync			 = 0;
	g_fullscreen	 = 0;
	g_logical_width	 = 0;
	g_logical_height = 0;
	g_window_width	 = width;
	g_window_height	 = height;
	return 0;
}

void bapi_engine_quit(void)
{
	bapi_runtime_stop();
	bapi_reset_transform();
	bapi_frame_reset();
	g_vsync			 = 0;
	g_fullscreen	 = 0;
	g_logical_width	 = 0;
	g_logical_height = 0;
	g_window_width	 = 0;
	g_window_height	 = 0;
	bapi_log_shutdown();
}

int bapi_poll_event(bapi_event_t *event)
{
	const plat_interface_t *plat = plat_get();
	if (!bapi_runtime_is_initialized() || plat == NULL) {
		if (event != NULL) event->type = BAPI_EVENT_UNKNOWN;
		return 0;
	}
	if (event == NULL) {
		return plat->window.poll_event(NULL);
	}
	plat_event_t plat_event;
	int			 has_event = plat->window.poll_event(&plat_event);
	if (has_event) {
		bapi_event_from_platform(event, &plat_event);
		if (event->type == BAPI_EVENT_WINDOW_RESIZED) {
			g_window_width	= event->data.window.width;
			g_window_height = event->data.window.height;
		}
	}
	return has_event;
}

int bapi_event_get_type(const bapi_event_t *event)
{
	return event != NULL ? event->type : BAPI_EVENT_UNKNOWN;
}

uint8_t bapi_event_get_key_code(const bapi_event_t *event)
{
	return event != NULL ? (uint8_t)event->data.key.key : 0;
}

int bapi_event_get_mouse_x(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.button.x : 0;
}

int bapi_event_get_mouse_y(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.button.y : 0;
}

int bapi_event_get_mouse_button(const bapi_event_t *event)
{
	return event != NULL ? event->data.button.button : 0;
}

int bapi_event_get_motion_x(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.motion.x : 0;
}

int bapi_event_get_motion_y(const bapi_event_t *event)
{
	return event != NULL ? (int)event->data.motion.y : 0;
}

float bapi_event_get_wheel_x(const bapi_event_t *event)
{
	return event != NULL ? event->data.wheel.x : 0.0f;
}

float bapi_event_get_wheel_y(const bapi_event_t *event)
{
	return event != NULL ? event->data.wheel.y : 0.0f;
}

float bapi_event_get_wheel_mouse_x(const bapi_event_t *event)
{
	return event != NULL ? event->data.wheel.mouse_x : 0.0f;
}

float bapi_event_get_wheel_mouse_y(const bapi_event_t *event)
{
	return event != NULL ? event->data.wheel.mouse_y : 0.0f;
}

int bapi_event_is_mouse_button_down(const bapi_event_t *event)
{
	return event != NULL && event->type == BAPI_EVENT_MOUSE_BUTTON_DOWN;
}

int bapi_event_is_mouse_button_up(const bapi_event_t *event)
{
	return event != NULL && event->type == BAPI_EVENT_MOUSE_BUTTON_UP;
}

int bapi_event_is_mouse_motion(const bapi_event_t *event)
{
	return event != NULL && event->type == BAPI_EVENT_MOUSE_MOTION;
}

int bapi_event_get_window_width(const bapi_event_t *event)
{
	return event != NULL ? event->data.window.width : 0;
}

int bapi_event_get_window_height(const bapi_event_t *event)
{
	return event != NULL ? event->data.window.height : 0;
}

void bapi_render_clear(void)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, 0, 0, 0, 255);
	plat->renderer.render_clear(renderer);
}

void bapi_render_present(void)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.render_present(renderer);
}

void bapi_set_render_color(bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
}

void bapi_delay(uint32_t ms)
{
	const plat_interface_t *plat = plat_get();
	if (!bapi_runtime_is_initialized() || plat == NULL) return;
	plat->core.delay(ms);
}

bapi_color_t bapi_color_from_hex(uint32_t hex_color)
{
	bapi_color_t color;
	color.r = (uint8_t)((hex_color >> 24) & 0xFF);
	color.g = (uint8_t)((hex_color >> 16) & 0xFF);
	color.b = (uint8_t)((hex_color >> 8) & 0xFF);
	color.a = (uint8_t)(hex_color & 0xFF);
	return color;
}

bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	bapi_color_t color = {r, g, b, a};
	return color;
}

uint32_t bapi_get_ticks(void)
{
	const plat_interface_t *plat = plat_get();
	if (!bapi_runtime_is_initialized() || plat == NULL) return 0;
	return plat->core.get_ticks();
}

void bapi_set_vsync(int enabled)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	if (plat->renderer.set_render_vsync == NULL) return;
	if (plat->renderer.set_render_vsync(renderer, enabled != 0) == 0) {
		g_vsync = enabled != 0;
	}
}

int bapi_get_vsync(void)
{
	return g_vsync;
}

void bapi_set_fullscreen(int enabled)
{
	const plat_interface_t *plat   = plat_get();
	plat_window_t			window = bapi_runtime_window();
	if (!bapi_runtime_is_initialized() || plat == NULL || window == NULL) return;
	if (plat->window.set_window_fullscreen == NULL) return;
	if (plat->window.set_window_fullscreen(window, enabled != 0) == 0) {
		g_fullscreen = enabled != 0;
	}
}

int bapi_get_fullscreen(void)
{
	return g_fullscreen;
}

void bapi_set_logical_size(int width, int height)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	if (width <= 0 || height <= 0) {
		width  = 0;
		height = 0;
	}
	if (plat->renderer.set_render_logical_size == NULL) return;
	if (plat->renderer.set_render_logical_size(renderer, width, height) == 0) {
		g_logical_width	 = width;
		g_logical_height = height;
	}
}

void bapi_get_logical_size(int *width, int *height)
{
	if (width) *width = g_logical_width;
	if (height) *height = g_logical_height;
}

void bapi_get_window_size(int *width, int *height)
{
	const plat_interface_t *plat   = plat_get();
	plat_window_t			window = bapi_runtime_window();
	int						w	   = g_window_width;
	int						h	   = g_window_height;

	if (bapi_runtime_is_initialized() && plat != NULL && window != NULL &&
		plat->window.get_window_size != NULL && plat->window.get_window_size(window, &w, &h) == 0) {
		g_window_width	= w;
		g_window_height = h;
	}
	if (width) *width = w;
	if (height) *height = h;
}

static int floor_to_int(float value)
{
	int result = (int)value;
	if ((float)result > value) result--;
	return result;
}

static int ceil_to_int(float value)
{
	int result = (int)value;
	if ((float)result < value) result++;
	return result;
}

static void fill_triangle_cpu(plat_renderer_t renderer, const bapi_vec2_t *a, const bapi_vec2_t *b,
							  const bapi_vec2_t *c, bapi_color_t color)
{
	float min_y = a->y < b->y ? (a->y < c->y ? a->y : c->y) : (b->y < c->y ? b->y : c->y);
	float max_y = a->y > b->y ? (a->y > c->y ? a->y : c->y) : (b->y > c->y ? b->y : c->y);

	int y_start = ceil_to_int(min_y);
	int y_end	= floor_to_int(max_y);
	if (y_end < y_start) return;

	const plat_interface_t *plat = plat_get();
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);

	for (int y = y_start; y <= y_end; y++) {
		float			   sample = (float)y + 0.5f;
		float			   xs[3];
		int				   count	   = 0;
		const bapi_vec2_t *vertices[3] = {a, b, c};

		for (int edge = 0; edge < 3; edge++) {
			const bapi_vec2_t *p0 = vertices[edge];
			const bapi_vec2_t *p1 = vertices[(edge + 1) % 3];
			if ((p0->y <= sample && p1->y > sample) || (p1->y <= sample && p0->y > sample)) {
				float t		= (sample - p0->y) / (p1->y - p0->y);
				xs[count++] = p0->x + t * (p1->x - p0->x);
			}
		}
		if (count < 2) continue;
		for (int i = 0; i < count; i++) {
			for (int j = i + 1; j < count; j++) {
				if (xs[j] < xs[i]) {
					float tmp = xs[i];
					xs[i]	  = xs[j];
					xs[j]	  = tmp;
				}
			}
		}
		if (xs[1] > xs[0]) {
			plat->renderer.render_fill_rect(renderer, xs[0], (float)y, xs[1] - xs[0], 1.0f);
		}
	}
}

static void fill_triangle_transformed(plat_renderer_t renderer, bapi_vec2_t a, bapi_vec2_t b,
									  bapi_vec2_t c, bapi_color_t color)
{
	const plat_interface_t *plat = plat_get();
	if (plat->renderer.render_fill_triangle != NULL) {
		plat->renderer.render_fill_triangle(renderer, a.x, a.y, b.x, b.y, c.x, c.y, color.r,
											color.g, color.b, color.a);
		return;
	}
	fill_triangle_cpu(renderer, &a, &b, &c, color);
}

void bapi_fill_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
						bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;

	bapi_vec2_t a = bapi_transform_point((bapi_vec2_t){x1, y1});
	bapi_vec2_t b = bapi_transform_point((bapi_vec2_t){x2, y2});
	bapi_vec2_t c = bapi_transform_point((bapi_vec2_t){x3, y3});
	fill_triangle_transformed(renderer, a, b, c, color);
}

void bapi_draw_pixel(float x, float y, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	bapi_vec2_t p = bapi_transform_point((bapi_vec2_t){x, y});
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_point(renderer, p.x, p.y);
}

void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	bapi_vec2_t a = bapi_transform_point((bapi_vec2_t){x1, y1});
	bapi_vec2_t b = bapi_transform_point((bapi_vec2_t){x2, y2});
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_line(renderer, a.x, a.y, b.x, b.y);
}

void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);

	if (bapi_transform_is_identity()) {
		plat->renderer.render_rect(renderer, x, y, w, h);
		return;
	}

	bapi_vec2_t p0 = bapi_transform_point((bapi_vec2_t){x, y});
	bapi_vec2_t p1 = bapi_transform_point((bapi_vec2_t){x + w, y});
	bapi_vec2_t p2 = bapi_transform_point((bapi_vec2_t){x + w, y + h});
	bapi_vec2_t p3 = bapi_transform_point((bapi_vec2_t){x, y + h});
	plat->renderer.render_line(renderer, p0.x, p0.y, p1.x, p1.y);
	plat->renderer.render_line(renderer, p1.x, p1.y, p2.x, p2.y);
	plat->renderer.render_line(renderer, p2.x, p2.y, p3.x, p3.y);
	plat->renderer.render_line(renderer, p3.x, p3.y, p0.x, p0.y);
}

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;

	if (bapi_transform_is_identity()) {
		plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
		plat->renderer.render_fill_rect(renderer, x, y, w, h);
		return;
	}

	bapi_vec2_t p0 = bapi_transform_point((bapi_vec2_t){x, y});
	bapi_vec2_t p1 = bapi_transform_point((bapi_vec2_t){x + w, y});
	bapi_vec2_t p2 = bapi_transform_point((bapi_vec2_t){x + w, y + h});
	bapi_vec2_t p3 = bapi_transform_point((bapi_vec2_t){x, y + h});
	fill_triangle_transformed(renderer, p0, p1, p2, color);
	fill_triangle_transformed(renderer, p0, p2, p3, color);
}

void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
						bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	bapi_vec2_t a = bapi_transform_point((bapi_vec2_t){x1, y1});
	bapi_vec2_t b = bapi_transform_point((bapi_vec2_t){x2, y2});
	bapi_vec2_t c = bapi_transform_point((bapi_vec2_t){x3, y3});
	plat->renderer.render_line(renderer, a.x, a.y, b.x, b.y);
	plat->renderer.render_line(renderer, b.x, b.y, c.x, c.y);
	plat->renderer.render_line(renderer, c.x, c.y, a.x, a.y);
}

void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	const int	segments   = 64;
	const float angle_step = 2.0f * (float)BAPI_PI / (float)segments;

	for (int i = 0; i < segments; i++) {
		float		angle1 = (float)i * angle_step;
		float		angle2 = (float)(i + 1) * angle_step;
		bapi_vec2_t p1	   = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle1) * radius, cy + bapi_sine(angle1) * radius));
		bapi_vec2_t p2 = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle2) * radius, cy + bapi_sine(angle2) * radius));
		plat->renderer.render_line(renderer, p1.x, p1.y, p2.x, p2.y);
	}
}

void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	const int	segments   = 64;
	const float angle_step = 2.0f * (float)BAPI_PI / (float)segments;
	bapi_vec2_t center	   = bapi_transform_point((bapi_vec2_t){cx, cy});

	for (int i = 0; i < segments; i++) {
		float		angle1 = (float)i * angle_step;
		float		angle2 = (float)(i + 1) * angle_step;
		bapi_vec2_t p1	   = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle1) * radius, cy + bapi_sine(angle1) * radius));
		bapi_vec2_t p2 = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle2) * radius, cy + bapi_sine(angle2) * radius));
		fill_triangle_transformed(renderer, center, p1, p2, color);
	}
}

void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color)
{
	if (sides < 3) return;

	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	float angle_step = 2.0f * (float)BAPI_PI / (float)sides;

	for (int i = 0; i < sides; i++) {
		float		angle1 = (float)i * angle_step - (float)BAPI_PI / 2;
		float		angle2 = (float)(i + 1) * angle_step - (float)BAPI_PI / 2;
		bapi_vec2_t p1	   = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle1) * radius, cy + bapi_sine(angle1) * radius));
		bapi_vec2_t p2 = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle2) * radius, cy + bapi_sine(angle2) * radius));
		plat->renderer.render_line(renderer, p1.x, p1.y, p2.x, p2.y);
	}
}

void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color)
{
	if (sides < 3) return;

	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	float		angle_step = 2.0f * (float)BAPI_PI / (float)sides;
	bapi_vec2_t center	   = bapi_transform_point((bapi_vec2_t){cx, cy});

	for (int i = 0; i < sides; i++) {
		float		angle1 = (float)i * angle_step - (float)BAPI_PI / 2;
		float		angle2 = (float)(i + 1) * angle_step - (float)BAPI_PI / 2;
		bapi_vec2_t p1	   = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle1) * radius, cy + bapi_sine(angle1) * radius));
		bapi_vec2_t p2 = bapi_transform_point(
			make_vec2(cx + bapi_cosine(angle2) * radius, cy + bapi_sine(angle2) * radius));
		fill_triangle_transformed(renderer, center, p1, p2, color);
	}
}
