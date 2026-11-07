#include "BridgeEngine.h"

#include <stdlib.h>

/* Camera center follows the target through a deadzone, clamped to world bounds.
 * Smoothing is a direct lerp factor clamped to [0,1]; shake is deterministic.
 */

struct bapi_camera_controller_internal {
	float viewport_w;
	float viewport_h;

	float x;
	float y;

	float target_x;
	float target_y;

	float deadzone_w;
	float deadzone_h;

	int			has_bounds;
	bapi_rect_t bounds;

	float smoothing;

	float shake_amplitude;
	float shake_duration;
	float shake_time;
	float shake_elapsed;
	float shake_offset_x;
	float shake_offset_y;
};

static int cc_is_nan(float v)
{
	return v != v;
}

static float cc_clampf(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

/* Deterministic triangle wave in [-1, 1], period 1 in phase. */
static float cc_triangle(float phase)
{
	float frac = phase - (float)(int)phase;
	if (frac < 0.0f) frac += 1.0f;
	if (frac < 0.5f) return 4.0f * frac - 1.0f;
	return 3.0f - 4.0f * frac;
}

static void cc_clamp_to_bounds(bapi_camera_controller_t c)
{
	float half_w;
	float half_h;

	if (!c->has_bounds) return;

	half_w = c->viewport_w * 0.5f;
	half_h = c->viewport_h * 0.5f;

	if (c->bounds.w >= c->viewport_w) {
		c->x = cc_clampf(c->x, c->bounds.x + half_w, c->bounds.x + c->bounds.w - half_w);
	} else {
		c->x = c->bounds.x + c->bounds.w * 0.5f;
	}

	if (c->bounds.h >= c->viewport_h) {
		c->y = cc_clampf(c->y, c->bounds.y + half_h, c->bounds.y + c->bounds.h - half_h);
	} else {
		c->y = c->bounds.y + c->bounds.h * 0.5f;
	}
}

/* Desired camera center for a target, after applying the deadzone. */
static float cc_desired_axis(float current, float target, float deadzone)
{
	float half;

	if (!(deadzone > 0.0f)) return target;

	half = deadzone * 0.5f;
	if (target > current + half) return target - half;
	if (target < current - half) return target + half;
	return current;
}

bapi_camera_controller_t bapi_camera_controller_create(float viewport_w, float viewport_h)
{
	bapi_camera_controller_t c;

	if (viewport_w < 0.0f) viewport_w = 0.0f;
	if (viewport_h < 0.0f) viewport_h = 0.0f;

	c = (bapi_camera_controller_t)calloc(1, sizeof(*c));
	if (!c) return NULL;

	c->viewport_w = viewport_w;
	c->viewport_h = viewport_h;
	c->x		  = 0.0f;
	c->y		  = 0.0f;
	c->target_x	  = 0.0f;
	c->target_y	  = 0.0f;
	return c;
}

void bapi_camera_controller_destroy(bapi_camera_controller_t controller)
{
	free(controller);
}

void bapi_camera_controller_set_bounds(bapi_camera_controller_t controller, bapi_rect_t bounds)
{
	if (!controller) return;

	controller->bounds	   = bounds;
	controller->has_bounds = bounds.w > 0.0f && bounds.h > 0.0f;
	cc_clamp_to_bounds(controller);
}

void bapi_camera_controller_set_deadzone(bapi_camera_controller_t controller, float width,
										 float height)
{
	if (!controller) return;

	controller->deadzone_w = width > 0.0f ? width : 0.0f;
	controller->deadzone_h = height > 0.0f ? height : 0.0f;
}

void bapi_camera_controller_set_smoothing(bapi_camera_controller_t controller, float smoothing)
{
	if (!controller) return;

	controller->smoothing = smoothing > 0.0f ? smoothing : 0.0f;
}

void bapi_camera_controller_set_viewport(bapi_camera_controller_t controller, float width,
										 float height)
{
	if (!controller) return;

	controller->viewport_w = width > 0.0f ? width : 0.0f;
	controller->viewport_h = height > 0.0f ? height : 0.0f;
	cc_clamp_to_bounds(controller);
}

void bapi_camera_controller_follow(bapi_camera_controller_t controller, float x, float y)
{
	if (!controller) return;

	if (!cc_is_nan(x)) controller->target_x = x;
	if (!cc_is_nan(y)) controller->target_y = y;
}

void bapi_camera_controller_snap(bapi_camera_controller_t controller)
{
	if (!controller) return;

	/* Snap ignores the deadzone: the camera center goes straight to the
	 * recorded follow target (then obeys world bounds). Active shake is left
	 * untouched and keeps decaying through subsequent updates. */
	controller->x = controller->target_x;
	controller->y = controller->target_y;
	cc_clamp_to_bounds(controller);
}

void bapi_camera_controller_update(bapi_camera_controller_t controller, float delta_time,
								   float target_x, float target_y)
{
	float desired_x;
	float desired_y;
	float factor;
	float dt;

	if (!controller) return;

	dt = delta_time;
	if (cc_is_nan(dt) || dt < 0.0f) dt = 0.0f;

	if (!cc_is_nan(target_x) && !cc_is_nan(target_y)) {
		controller->target_x = target_x;
		controller->target_y = target_y;
	}

	if (controller->smoothing > 0.0f) {
		factor = controller->smoothing * dt;
		factor = cc_clampf(factor, 0.0f, 1.0f);
	} else {
		factor = 1.0f;
	}

	desired_x = cc_desired_axis(controller->x, controller->target_x, controller->deadzone_w);
	desired_y = cc_desired_axis(controller->y, controller->target_y, controller->deadzone_h);

	controller->x += (desired_x - controller->x) * factor;
	controller->y += (desired_y - controller->y) * factor;

	cc_clamp_to_bounds(controller);

	/* Deterministic shake: linear amplitude decay plus triangle-wave motion. */
	if (controller->shake_time > 0.0f && controller->shake_duration > 0.0f) {
		float decay;
		float phase;

		controller->shake_elapsed += dt;
		controller->shake_time -= dt;
		if (controller->shake_time < 0.0f) controller->shake_time = 0.0f;

		decay = controller->shake_time / controller->shake_duration;
		phase = controller->shake_elapsed * 6.0f;

		controller->shake_offset_x = controller->shake_amplitude * decay * cc_triangle(phase);
		controller->shake_offset_y =
			controller->shake_amplitude * decay * cc_triangle(phase + 0.25f);
	} else {
		controller->shake_offset_x = 0.0f;
		controller->shake_offset_y = 0.0f;
	}
}

void bapi_camera_controller_get_camera(bapi_camera_controller_t controller, bapi_camera_t *out)
{
	if (!out) return;

	if (!controller) {
		*out = (bapi_camera_t){0};
		return;
	}

	out->x			= controller->x + controller->shake_offset_x;
	out->y			= controller->y + controller->shake_offset_y;
	out->zoom		= 1.0f;
	out->rotation	= 0.0f;
	out->viewport_w = controller->viewport_w;
	out->viewport_h = controller->viewport_h;
}

void bapi_camera_controller_shake(bapi_camera_controller_t controller, float amplitude,
								  float duration)
{
	if (!controller) return;

	if (!(amplitude > 0.0f) || !(duration > 0.0f)) {
		controller->shake_amplitude = 0.0f;
		controller->shake_duration	= 0.0f;
		controller->shake_time		= 0.0f;
		controller->shake_elapsed	= 0.0f;
		controller->shake_offset_x	= 0.0f;
		controller->shake_offset_y	= 0.0f;
		return;
	}

	controller->shake_amplitude = amplitude;
	controller->shake_duration	= duration;
	controller->shake_time		= duration;
	controller->shake_elapsed	= 0.0f;
	controller->shake_offset_x	= 0.0f;
	controller->shake_offset_y	= 0.0f;
}
