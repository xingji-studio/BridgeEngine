#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/math.h"
#include "internal/engine/render_context.h"
#include "internal/engine/texture_internal.h"
#include "internal/platform/platform.h"
#include <math.h>
#include <stdlib.h>

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
	default:
		dst->type = BAPI_EVENT_UNKNOWN;
		break;
	}
}

static float bapi_sine(float angle)
{
#ifdef __XJ380_OS__
	const float pi = (float)BAPI_PI;
	const float half_pi = pi / 2.0f;
	const float two_pi = pi * 2.0f;
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

bapi_window_t bapi_engine_get_window(void)
{
	return bapi_runtime_is_initialized() ? (bapi_window_t)bapi_runtime_window() : NULL;
}

bapi_renderer_t bapi_engine_get_renderer(void)
{
	return bapi_runtime_is_initialized() ? (bapi_renderer_t)bapi_runtime_renderer() : NULL;
}

int bapi_engine_init(const char *title, int width, int height)
{
#if defined(USE_BACKEND_MINIOS)
	const plat_interface_t *platform = plat_minios_interface();
#elif defined(USE_BACKEND_XJ380)
	const plat_interface_t *platform = plat_xj380_interface();
#else
	const plat_interface_t *platform = plat_sdl3_interface();
#endif
	return bapi_runtime_start(platform, title, width, height);
}

void bapi_engine_quit(void)
{
	bapi_runtime_stop();
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

void bapi_draw_pixel(float x, float y, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_point(renderer, x, y);
}

void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_line(renderer, x1, y1, x2, y2);
}

void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_rect(renderer, x, y, w, h);
}

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_fill_rect(renderer, x, y, w, h);
}

void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
						bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	plat->renderer.render_line(renderer, x1, y1, x2, y2);
	plat->renderer.render_line(renderer, x2, y2, x3, y3);
	plat->renderer.render_line(renderer, x3, y3, x1, y1);
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
		float angle1 = (float)i * angle_step;
		float angle2 = (float)(i + 1) * angle_step;

		float x1 = cx + bapi_cosine(angle1) * radius;
		float y1 = cy + bapi_sine(angle1) * radius;
		float x2 = cx + bapi_cosine(angle2) * radius;
		float y2 = cy + bapi_sine(angle2) * radius;

		plat->renderer.render_line(renderer, x1, y1, x2, y2);
	}
}

void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	const int	segments   = 64;
	const float angle_step = 2.0f * (float)BAPI_PI / (float)segments;

	for (int i = 0; i < segments; i++) {
		float angle1 = (float)i * angle_step;
		float angle2 = (float)(i + 1) * angle_step;

		float x1 = cx + bapi_cosine(angle1) * radius;
		float y1 = cy + bapi_sine(angle1) * radius;
		float x2 = cx + bapi_cosine(angle2) * radius;
		float y2 = cy + bapi_sine(angle2) * radius;

		plat->renderer.render_line(renderer, cx, cy, x1, y1);
		plat->renderer.render_line(renderer, x1, y1, x2, y2);
		plat->renderer.render_line(renderer, x2, y2, cx, cy);
	}
}

void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color)
{
	if (sides < 3) {
		return;
	}

	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	float angle_step = 2.0f * (float)BAPI_PI / (float)sides;

	for (int i = 0; i < sides; i++) {
		float angle1 = (float)i * angle_step - (float)BAPI_PI / 2;
		float angle2 = (float)(i + 1) * angle_step - (float)BAPI_PI / 2;

		float x1 = cx + bapi_cosine(angle1) * radius;
		float y1 = cy + bapi_sine(angle1) * radius;
		float x2 = cx + bapi_cosine(angle2) * radius;
		float y2 = cy + bapi_sine(angle2) * radius;

		plat->renderer.render_line(renderer, x1, y1, x2, y2);
	}
}

void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color)
{
	if (sides < 3) {
		return;
	}

	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL) return;
	plat->renderer.set_render_draw_color(renderer, color.r, color.g, color.b, color.a);
	float angle_step = 2.0f * (float)BAPI_PI / (float)sides;

	for (int i = 0; i < sides; i++) {
		float angle1 = (float)i * angle_step - (float)BAPI_PI / 2;
		float angle2 = (float)(i + 1) * angle_step - (float)BAPI_PI / 2;

		float x1 = cx + bapi_cosine(angle1) * radius;
		float y1 = cy + bapi_sine(angle1) * radius;
		float x2 = cx + bapi_cosine(angle2) * radius;
		float y2 = cy + bapi_sine(angle2) * radius;

		plat->renderer.render_line(renderer, cx, cy, x1, y1);
		plat->renderer.render_line(renderer, x1, y1, x2, y2);
		plat->renderer.render_line(renderer, x2, y2, cx, cy);
	}
}

void bapi_draw_image(const char *filepath, float x, float y, float w, float h)
{
	const plat_interface_t *plat	 = plat_get();
	plat_renderer_t			renderer = bapi_internal_get_renderer();
	if (!bapi_runtime_is_initialized() || plat == NULL || renderer == NULL || filepath == NULL) return;

	plat_surface_t *surface = plat->texture.load_image(filepath);
	if (surface == NULL) {
		return;
	}

	bapi_texture_t texture = malloc(sizeof(struct bapi_texture_internal));
	if (texture == NULL) {
		plat->texture.destroy_surface(surface);
		return;
	}

	texture->platform_texture = plat->texture.create_texture_from_surface(renderer, surface);
	plat->texture.destroy_surface(surface);

	if (texture->platform_texture == NULL) {
		free(texture);
		return;
	}

	plat->renderer.render_texture(renderer, texture->platform_texture, x, y, w, h);

	plat->texture.destroy_texture(texture->platform_texture);
	free(texture);
}
