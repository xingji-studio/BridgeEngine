#include "internal/engine/frame.h"
#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/render_context.h"
#include "internal/platform/platform.h"

#include <stdint.h>
#include <string.h>

#define BAPI_FRAME_MAX_DELTA 0.25f

typedef struct {
	uint64_t last_ns;
	uint64_t frame_start_ns;
	uint64_t frame_count;
	double	 elapsed;
	double	 real_delta;
	float	 delta;
	float	 fps;
	float	 time_scale;
	float	 step_accumulator;
	float	 step_size;
	int		 target_fps;
	int		 paused;
	int		 have_last;
	int		 step_delta_consumed;
} bapi_frame_state_t;

static bapi_frame_state_t g_frame;

static uint64_t frame_now_ns(void)
{
	const plat_interface_t *plat = plat_get();
	if (plat == NULL) return 0;
	if (plat->core.get_time_ns != NULL) return plat->core.get_time_ns();
	if (plat->core.get_ticks != NULL) return (uint64_t)plat->core.get_ticks() * 1000000ull;
	return 0;
}

void bapi_frame_reset(void)
{
	memset(&g_frame, 0, sizeof(g_frame));
	g_frame.time_scale = 1.0f;
	g_frame.step_size  = 1.0f / 60.0f;
}

float bapi_frame_begin(void)
{
	if (!bapi_runtime_is_initialized()) return 0.0f;

	uint64_t now = frame_now_ns();
	if (!g_frame.have_last) {
		g_frame.have_last	   = 1;
		g_frame.last_ns		   = now;
		g_frame.frame_start_ns = now;
		g_frame.delta		   = 0.0f;
		g_frame.frame_count++;
		return 0.0f;
	}

	double raw_delta = (double)(now - g_frame.last_ns) / 1000000000.0;
	if (raw_delta < 0.0) raw_delta = 0.0;
	if (raw_delta > BAPI_FRAME_MAX_DELTA) raw_delta = BAPI_FRAME_MAX_DELTA;

	g_frame.last_ns		   = now;
	g_frame.frame_start_ns = now;
	g_frame.real_delta	   = raw_delta;

	if (raw_delta > 0.0) {
		float instant_fps = (float)(1.0 / raw_delta);
		if (g_frame.fps <= 0.0f) {
			g_frame.fps = instant_fps;
		} else {
			g_frame.fps = g_frame.fps * 0.9f + instant_fps * 0.1f;
		}
	}

	if (g_frame.paused) {
		g_frame.delta = 0.0f;
	} else {
		g_frame.delta = (float)raw_delta * g_frame.time_scale;
		g_frame.elapsed += (double)g_frame.delta;
	}

	g_frame.step_delta_consumed = 0;
	g_frame.frame_count++;
	return g_frame.delta;
}

void bapi_frame_end(void)
{
	if (!bapi_runtime_is_initialized()) return;

	bapi_render_present();

	if (g_frame.target_fps > 0) {
		uint64_t budget_ns = 1000000000ull / (uint64_t)g_frame.target_fps;
		uint64_t now	   = frame_now_ns();
		uint64_t spent	   = now - g_frame.frame_start_ns;
		if (spent < budget_ns) {
			uint64_t remaining_ms = (budget_ns - spent) / 1000000ull;
			if (remaining_ms > 0) bapi_delay((uint32_t)remaining_ms);
		}
	}
}

float bapi_get_delta_time(void)
{
	return g_frame.delta;
}

float bapi_get_fps(void)
{
	return g_frame.fps;
}

double bapi_get_elapsed_time(void)
{
	return g_frame.elapsed;
}

uint64_t bapi_get_frame_count(void)
{
	return g_frame.frame_count;
}

void bapi_set_target_fps(int fps)
{
	if (fps < 0) fps = 0;
	if (fps > 1000) fps = 1000;
	g_frame.target_fps = fps;
}

void bapi_set_time_scale(float scale)
{
	if (scale != scale || scale < 0.0f) scale = 0.0f;
	if (scale > 100.0f) scale = 100.0f;
	g_frame.time_scale = scale;
}

void bapi_set_frame_paused(int paused)
{
	g_frame.paused = paused != 0;
}

int bapi_frame_is_paused(void)
{
	return g_frame.paused;
}

int bapi_frame_steps(float fixed_delta, int max_steps)
{
	if (!(fixed_delta > 0.0f) || max_steps <= 0 || g_frame.paused) return 0;

	g_frame.step_size = fixed_delta;
	if (!g_frame.step_delta_consumed) {
		g_frame.step_accumulator += g_frame.delta;
		g_frame.step_delta_consumed = 1;
	}

	int steps = 0;
	while (g_frame.step_accumulator >= fixed_delta && steps < max_steps) {
		g_frame.step_accumulator -= fixed_delta;
		steps++;
	}
	if (steps == max_steps) {
		/* Avoid a spiral of death: drop the backlog after the cap. */
		g_frame.step_accumulator = 0.0f;
	}
	return steps;
}

float bapi_frame_step_alpha(void)
{
	if (!(g_frame.step_size > 0.0f)) return 0.0f;
	float alpha = g_frame.step_accumulator / g_frame.step_size;
	if (alpha < 0.0f) alpha = 0.0f;
	if (alpha >= 1.0f) alpha = 1.0f - 1e-6f;
	return alpha;
}
