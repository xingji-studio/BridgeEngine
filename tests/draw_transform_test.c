#include "BridgeEngine.h"
#include "internal/engine/render_context.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdio.h>

struct plat_window {
	int unused;
};

struct plat_renderer {
	int unused;
};

static struct plat_window	  fake_window;
static struct plat_renderer	  fake_renderer;
static const plat_interface_t test_platform;

static float last_x, last_y, last_x2, last_y2;
static int	 fill_rect_count;
static int	 fill_triangle_count;
static int	 render_count;
static int	 vsync_value	  = -1;
static int	 fullscreen_value = -1;
static int	 logical_w, logical_h;
static int	 window_w = 640, window_h = 360;
static int	 resize_pending;

void bapi_video_cleanup(void) {}
void bapi_audio_cleanup(void) {}
void bapi_texture_cleanup(void) {}
void bapi_text_cleanup(void) {}
void bapi_log_shutdown(void) {}

const plat_interface_t *plat_sdl3_interface(void)
{
	return &test_platform;
}

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

static int core_init(uint32_t flags)
{
	(void)flags;
	return 0;
}

static void core_quit(void) {}

static void delay(uint32_t ms)
{
	(void)ms;
}

static uint32_t get_ticks(void)
{
	return 0;
}

static plat_window_t create_window(const char *title, int width, int height)
{
	(void)title;
	(void)width;
	(void)height;
	return &fake_window;
}

static void destroy_window(plat_window_t window)
{
	(void)window;
}

static int poll_event(plat_event_t *event)
{
	if (!resize_pending) return 0;
	resize_pending			  = 0;
	event->type				  = PLAT_EVENT_WINDOW_RESIZED;
	event->data.window.width  = 800;
	event->data.window.height = 450;
	return 1;
}

static plat_renderer_t create_renderer(plat_window_t window)
{
	return window != NULL ? &fake_renderer : NULL;
}

static void destroy_renderer(plat_renderer_t renderer)
{
	(void)renderer;
}

static void set_render_draw_color(plat_renderer_t renderer, uint8_t r, uint8_t g, uint8_t b,
								  uint8_t a)
{
	(void)renderer;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
}

static void set_render_draw_blend_mode(plat_renderer_t renderer, plat_blend_mode_t mode)
{
	(void)renderer;
	(void)mode;
}

static void render_noop(plat_renderer_t renderer)
{
	(void)renderer;
}

static void render_point(plat_renderer_t renderer, float x, float y)
{
	(void)renderer;
	last_x = x;
	last_y = y;
	render_count++;
}

static void render_line(plat_renderer_t renderer, float x1, float y1, float x2, float y2)
{
	(void)renderer;
	last_x	= x1;
	last_y	= y1;
	last_x2 = x2;
	last_y2 = y2;
	render_count++;
}

static void render_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	(void)renderer;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	render_count++;
}

static void render_fill_rect(plat_renderer_t renderer, float x, float y, float w, float h)
{
	(void)renderer;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	fill_rect_count++;
}

static void render_fill_triangle(plat_renderer_t renderer, float x1, float y1, float x2, float y2,
								 float x3, float y3, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	(void)renderer;
	(void)x2;
	(void)y2;
	(void)x3;
	(void)y3;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
	last_x = x1;
	last_y = y1;
	fill_triangle_count++;
}

static int set_window_fullscreen(plat_window_t window, int enabled)
{
	(void)window;
	fullscreen_value = enabled;
	return 0;
}

static int get_window_size(plat_window_t window, int *width, int *height)
{
	(void)window;
	*width	= window_w;
	*height = window_h;
	return 0;
}

static int set_render_vsync(plat_renderer_t renderer, int enabled)
{
	(void)renderer;
	vsync_value = enabled;
	return 0;
}

static int set_render_logical_size(plat_renderer_t renderer, int width, int height)
{
	(void)renderer;
	logical_w = width;
	logical_h = height;
	return 0;
}

static int init_ttf(void)
{
	return 0;
}

static void quit_ttf(void) {}

static const plat_interface_t test_platform = {
	.core	  = {.init = core_init, .quit = core_quit, .delay = delay, .get_ticks = get_ticks},
	.window	  = {.create_window			= create_window,
				 .destroy_window		= destroy_window,
				 .poll_event			= poll_event,
				 .set_window_fullscreen = set_window_fullscreen,
				 .get_window_size		= get_window_size},
	.renderer = {.create_renderer			 = create_renderer,
				 .destroy_renderer			 = destroy_renderer,
				 .set_render_draw_color		 = set_render_draw_color,
				 .set_render_draw_blend_mode = set_render_draw_blend_mode,
				 .render_clear				 = render_noop,
				 .render_present			 = render_noop,
				 .render_point				 = render_point,
				 .render_line				 = render_line,
				 .render_rect				 = render_rect,
				 .render_fill_rect			 = render_fill_rect,
				 .set_render_vsync			 = set_render_vsync,
				 .set_render_logical_size	 = set_render_logical_size,
				 .render_fill_triangle		 = render_fill_triangle},
	.text	  = {.init_ttf = init_ttf, .quit_ttf = quit_ttf},
};

int main(void)
{
	int result = 1;
	if (bapi_engine_init("draw-test", 320, 200) != 0) return 1;

	int ww = 0, hh = 0;
	bapi_get_window_size(&ww, &hh);
	result &= expect(ww == 640 && hh == 360, "window size is queryable");

	bapi_draw_pixel(10, 20, bapi_color(1, 2, 3, 4));
	result &= expect(last_x == 10.0f && last_y == 20.0f, "identity transform preserves points");

	bapi_push_transform((bapi_vec2_t){5.0f, 7.0f}, 0.0f, (bapi_vec2_t){1.0f, 1.0f});
	bapi_draw_pixel(10, 20, bapi_color(1, 2, 3, 4));
	result &= expect(last_x == 15.0f && last_y == 27.0f, "translation is applied");
	bapi_vec2_t point = bapi_transform_point((bapi_vec2_t){1.0f, 1.0f});
	result &= expect(point.x == 6.0f && point.y == 8.0f, "transform_point matches draw path");
	bapi_pop_transform();

	bapi_push_transform((bapi_vec2_t){0.0f, 0.0f}, 0.0f, (bapi_vec2_t){2.0f, 3.0f});
	result &= expect(fabsf(bapi_transform_scale_factor() - sqrtf(6.0f)) < 0.0001f,
					 "scale factor is the geometric mean");
	bapi_draw_line(1, 1, 2, 2, bapi_color(1, 2, 3, 4));
	result &= expect(last_x == 2.0f && last_y == 3.0f && last_x2 == 4.0f && last_y2 == 6.0f,
					 "lines transform both endpoints");
	bapi_pop_transform();

	bapi_camera_t camera;
	bapi_camera_init(&camera, 800.0f, 600.0f);
	bapi_camera_set_position(&camera, 100.0f, 200.0f);
	bapi_camera_set_zoom(&camera, 2.0f);
	bapi_apply_camera(&camera);
	point = bapi_transform_point((bapi_vec2_t){100.0f, 200.0f});
	result &= expect(fabsf(point.x - 400.0f) < 0.001f && fabsf(point.y - 300.0f) < 0.001f,
					 "camera maps its focus point to the viewport center");
	bapi_pop_transform();

	bapi_reset_transform();
	bapi_camera_init(&camera, 800.0f, 600.0f);
	bapi_camera_set_rotation(&camera, 1.57079632679f);
	bapi_apply_camera(&camera);
	point = bapi_transform_point((bapi_vec2_t){1.0f, 0.0f});
	result &= expect(fabsf(point.x - 400.0f) < 0.001f && fabsf(point.y - 299.0f) < 0.001f,
					 "camera rotation follows the documented world-to-screen formula");
	bapi_pop_transform();

	fill_triangle_count = 0;
	bapi_fill_triangle(0, 0, 10, 0, 0, 10, bapi_color(1, 2, 3, 4));
	result &= expect(fill_triangle_count == 1, "fill_triangle uses the platform fill path");

	fill_triangle_count = 0;
	bapi_fill_circle(50, 50, 20, bapi_color(1, 2, 3, 4));
	result &= expect(fill_triangle_count == 64, "fill_circle draws a real triangle fan");

	fill_triangle_count = 0;
	bapi_fill_polygon(50, 50, 20, 6, bapi_color(1, 2, 3, 4));
	result &= expect(fill_triangle_count == 6, "fill_polygon draws one triangle per side");

	fill_rect_count = 0;
	bapi_fill_rect(0, 0, 10, 10, bapi_color(1, 2, 3, 4));
	result &= expect(fill_rect_count == 1, "identity fill_rect stays a single platform call");

	fill_triangle_count = 0;
	bapi_push_transform((bapi_vec2_t){5.0f, 5.0f}, 0.0f, (bapi_vec2_t){2.0f, 2.0f});
	bapi_fill_rect(0, 0, 10, 10, bapi_color(1, 2, 3, 4));
	result &= expect(fill_triangle_count == 2, "transformed fill_rect uses two triangles");
	bapi_pop_transform();

	bapi_set_vsync(1);
	bapi_set_fullscreen(1);
	bapi_set_logical_size(800, 600);
	result &= expect(bapi_get_vsync() == 1, "vsync state is reported");
	result &= expect(bapi_get_fullscreen() == 1, "fullscreen state is reported");
	int lw = 0, lh = 0;
	bapi_get_logical_size(&lw, &lh);
	result &= expect(lw == 800 && lh == 600, "logical size is reported");

	resize_pending = 1;
	bapi_event_t event;
	result &= expect(bapi_poll_event(&event) == 1, "resize event is delivered");
	result &= expect(event.type == BAPI_EVENT_WINDOW_RESIZED, "resize event type is mapped");
	result &= expect(bapi_event_get_window_width(&event) == 800 &&
						 bapi_event_get_window_height(&event) == 450,
					 "resize dimensions are exposed");

	bapi_engine_quit();
	return result ? 0 : 1;
}
