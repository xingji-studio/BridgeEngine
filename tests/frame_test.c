#include "BridgeEngine.h"
#include "internal/engine/render_context.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct plat_window {
	int unused;
};

struct plat_renderer {
	int unused;
};

static struct plat_window	  fake_window;
static struct plat_renderer	  fake_renderer;
static uint64_t				  g_now_ns;
static int					  delay_count;
static int					  present_count;
static int					  requested_backend = -1;
static int					  actual_backend	 = 1;
static const plat_interface_t test_platform;

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
	delay_count++;
}

static uint32_t get_ticks(void)
{
	return (uint32_t)(g_now_ns / 1000000ull);
}

static uint64_t get_time_ns(void)
{
	return g_now_ns;
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
	(void)event;
	return 0;
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

static void render_present(plat_renderer_t renderer)
{
	(void)renderer;
	present_count++;
}

static int set_renderer_backend(int backend)
{
	requested_backend = backend;
	actual_backend	  = backend;
	return 0;
}

static int get_renderer_backend(void)
{
	return actual_backend;
}

static int init_ttf(void)
{
	return 0;
}

static void quit_ttf(void) {}

static const plat_interface_t test_platform = {
	.core	  = {.init		  = core_init,
				 .quit		  = core_quit,
				 .delay		  = delay,
				 .get_ticks	  = get_ticks,
				 .get_time_ns = get_time_ns},
	.window	  = {.create_window	 = create_window,
				 .destroy_window = destroy_window,
				 .poll_event	 = poll_event},
	.renderer = {.create_renderer			 = create_renderer,
				 .destroy_renderer			 = destroy_renderer,
				 .set_render_draw_color		 = set_render_draw_color,
				 .set_render_draw_blend_mode = set_render_draw_blend_mode,
				 .render_clear				 = render_noop,
				 .render_present			 = render_present},
	.text	  = {.init_ttf = init_ttf, .quit_ttf = quit_ttf},
	.set_renderer_backend = set_renderer_backend,
	.get_renderer_backend = get_renderer_backend,
};

int main(void)
{
	int result = 1;

	result &= expect(bapi_frame_begin() == 0.0f, "frame_begin is inert before init");
	result &= expect(bapi_get_frame_count() == 0, "frame count starts at zero");

	if (bapi_engine_init_ex("frame-test", 320, 200, BAPI_RENDERER_VULKAN) != 0) {
		fprintf(stderr, "engine init failed\n");
		return 1;
	}
	result &= expect(requested_backend == BAPI_RENDERER_VULKAN,
					 "requested backend reaches the platform");
	result &= expect(bapi_engine_get_renderer_backend() == BAPI_RENDERER_VULKAN,
					 "actual backend is reported");

	result &= expect(strcmp(bapi_get_last_error(), "") == 0, "successful init clears the error");
	result &= expect(bapi_frame_begin() == 0.0f, "first frame has zero delta");
	result &= expect(bapi_get_frame_count() == 1, "first frame increments the count");

	bapi_set_vsync(1);
	bapi_set_fullscreen(1);
	bapi_set_logical_size(160, 100);
	int logical_w = -1, logical_h = -1;
	bapi_get_logical_size(&logical_w, &logical_h);
	int window_w = -1, window_h = -1;
	bapi_get_window_size(&window_w, &window_h);
	result &= expect(bapi_get_vsync() == 0 && bapi_get_fullscreen() == 0,
					 "unsupported display slots leave state unchanged");
	result &= expect(logical_w == 0 && logical_h == 0,
					 "unsupported logical-size slot is a no-op");
	result &= expect(window_w == 320 && window_h == 200,
					 "window size falls back to the init size");

	g_now_ns += 16000000ull;
	float delta = bapi_frame_begin();
	result &= expect(fabsf(delta - 0.016f) < 0.0005f, "delta reflects elapsed time");
	result &= expect(fabs(bapi_get_elapsed_time() - 0.016) < 0.001, "elapsed advances");
	result &= expect(bapi_get_fps() > 50.0f && bapi_get_fps() < 70.0f, "fps estimate is sane");

	bapi_set_time_scale(2.0f);
	g_now_ns += 10000000ull;
	delta = bapi_frame_begin();
	result &= expect(fabsf(delta - 0.020f) < 0.0005f, "time scale multiplies delta");

	bapi_set_frame_paused(1);
	double elapsed = bapi_get_elapsed_time();
	g_now_ns += 10000000ull;
	delta = bapi_frame_begin();
	result &= expect(delta == 0.0f, "paused frames have zero delta");
	result &= expect(bapi_get_elapsed_time() == elapsed, "paused frames do not advance elapsed");

	bapi_set_frame_paused(0);
	bapi_set_time_scale(1.0f);
	g_now_ns += 50000000ull;
	(void)bapi_frame_begin();
	int steps = bapi_frame_steps(0.02f, 10);
	result &= expect(steps == 2, "fixed-step helper consumes two steps");
	result &= expect(fabsf(bapi_frame_step_alpha() - 0.5f) < 0.01f, "step alpha retains remainder");

	delay_count	  = 0;
	present_count = 0;
	bapi_set_target_fps(60);
	g_now_ns += 1000000ull;
	(void)bapi_frame_begin();
	bapi_frame_end();
	result &= expect(present_count == 1, "frame_end presents once");
	result &= expect(delay_count > 0, "frame_end applies the target-fps cap");

	bapi_engine_quit();
	result &= expect(bapi_frame_begin() == 0.0f, "frame_begin is inert after quit");

	return result ? 0 : 1;
}
