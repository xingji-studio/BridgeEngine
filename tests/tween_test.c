/* Tween/easing tests. */

#include "BridgeEngine.h"

#include <stdio.h>

static int failures;

static int expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
	return condition;
}

static int feq(float a, float b)
{
	float diff = a - b;
	if (diff < 0.0f) diff = -diff;
	return diff <= 1e-5f;
}

static const bapi_ease_t k_all_eases[] = {
	BAPI_EASE_LINEAR,	BAPI_EASE_IN_QUAD,	 BAPI_EASE_OUT_QUAD,	 BAPI_EASE_IN_OUT_QUAD,
	BAPI_EASE_IN_CUBIC, BAPI_EASE_OUT_CUBIC, BAPI_EASE_IN_OUT_CUBIC, BAPI_EASE_OUT_BACK,
};

static void test_ease_endpoints(void)
{
	unsigned i;

	for (i = 0; i < sizeof(k_all_eases) / sizeof(k_all_eases[0]); i++) {
		bapi_ease_t ease = k_all_eases[i];
		expect(feq(bapi_ease(ease, 0.0f), 0.0f), "ease(0) == 0");
		expect(feq(bapi_ease(ease, 1.0f), 1.0f), "ease(1) == 1");
	}
}

static void test_ease_mid_values(void)
{
	expect(feq(bapi_ease(BAPI_EASE_LINEAR, 0.5f), 0.5f), "linear mid");
	expect(feq(bapi_ease(BAPI_EASE_IN_QUAD, 0.5f), 0.25f), "in quad mid");
	expect(feq(bapi_ease(BAPI_EASE_OUT_QUAD, 0.5f), 0.75f), "out quad mid");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_QUAD, 0.5f), 0.5f), "in-out quad mid");
	expect(feq(bapi_ease(BAPI_EASE_IN_CUBIC, 0.5f), 0.125f), "in cubic mid");
	expect(feq(bapi_ease(BAPI_EASE_OUT_CUBIC, 0.5f), 0.875f), "out cubic mid");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_CUBIC, 0.5f), 0.5f), "in-out cubic mid");
	expect(feq(bapi_ease(BAPI_EASE_OUT_BACK, 0.5f), 1.0876975f), "out back mid");

	/* Quarter points for the non-symmetric curves. */
	expect(feq(bapi_ease(BAPI_EASE_IN_QUAD, 0.25f), 0.0625f), "in quad quarter");
	expect(feq(bapi_ease(BAPI_EASE_OUT_QUAD, 0.25f), 0.4375f), "out quad quarter");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_QUAD, 0.25f), 0.125f), "in-out quad first half");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_QUAD, 0.75f), 0.875f), "in-out quad second half");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_CUBIC, 0.25f), 0.0625f), "in-out cubic first half");
	expect(feq(bapi_ease(BAPI_EASE_IN_OUT_CUBIC, 0.75f), 0.9375f), "in-out cubic second half");
}

static void test_ease_clamp(void)
{
	expect(feq(bapi_ease(BAPI_EASE_LINEAR, -0.5f), 0.0f), "linear clamps below 0");
	expect(feq(bapi_ease(BAPI_EASE_LINEAR, 1.5f), 1.0f), "linear clamps above 1");
	expect(feq(bapi_ease(BAPI_EASE_IN_QUAD, -2.0f), 0.0f), "quad clamps below 0");
	expect(feq(bapi_ease(BAPI_EASE_OUT_CUBIC, 3.0f), 1.0f), "cubic clamps above 1");
	expect(feq(bapi_ease(BAPI_EASE_OUT_BACK, -1.0f), 0.0f), "out back clamps below 0");
	expect(feq(bapi_ease(BAPI_EASE_OUT_BACK, 2.0f), 1.0f), "out back clamps above 1");

	/* Unknown enum values fall back to linear. */
	expect(feq(bapi_ease((bapi_ease_t)999, 0.25f), 0.25f), "unknown ease falls back to linear");
}

static void test_out_back_overshoot(void)
{
	float max_value = 0.0f;
	int	  i;

	for (i = 1; i < 100; i++) {
		float v = bapi_ease(BAPI_EASE_OUT_BACK, (float)i / 100.0f);
		if (v > max_value) max_value = v;
	}
	/* Documented: eases past 1 mid-flight, then settles back at 1. */
	expect(max_value > 1.02f, "out back overshoots 1");
	expect(max_value < 1.2f, "out back overshoot stays small");
	expect(feq(bapi_ease(BAPI_EASE_OUT_BACK, 1.0f), 1.0f), "out back settles at 1");
}

static void test_tween_linear_interpolation(void)
{
	bapi_tween_manager_t manager = bapi_tween_manager_create();
	bapi_tween_t		 tween;

	expect(manager != NULL, "manager create");
	tween = bapi_tween_create(manager, 10.0f, 20.0f, 2.0f, BAPI_EASE_LINEAR, 0);
	expect(tween != NULL, "tween create");
	expect(feq(bapi_tween_value(tween), 10.0f), "tween starts at from");
	expect(bapi_tween_is_finished(tween) == 0, "new tween not finished");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 12.5f), "linear tween at 25%");
	expect(bapi_tween_is_finished(tween) == 0, "tween still running");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 15.0f), "linear tween at 50%");

	bapi_tween_manager_update(manager, 1.0f);
	expect(feq(bapi_tween_value(tween), 20.0f), "linear tween completes at to");
	expect(bapi_tween_is_finished(tween) == 1, "tween finished after duration");

	/* Further updates keep a finished non-loop tween clamped. */
	bapi_tween_manager_update(manager, 5.0f);
	expect(feq(bapi_tween_value(tween), 20.0f), "finished tween stays at to");
	expect(bapi_tween_is_finished(tween) == 1, "finished tween stays finished");

	bapi_tween_manager_destroy(manager);
}

static void test_tween_eased_value(void)
{
	bapi_tween_manager_t manager = bapi_tween_manager_create();
	bapi_tween_t		 tween;

	expect(manager != NULL, "manager create for eased tween");
	tween = bapi_tween_create(manager, 0.0f, 100.0f, 1.0f, BAPI_EASE_IN_QUAD, 0);
	expect(tween != NULL, "eased tween create");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 25.0f), "in-quad tween at 50%");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 100.0f), "eased tween completes");

	bapi_tween_manager_destroy(manager);
}

static void test_loop_wrap(void)
{
	bapi_tween_manager_t manager = bapi_tween_manager_create();
	bapi_tween_t		 tween;

	expect(manager != NULL, "manager create for loop");
	tween = bapi_tween_create(manager, 0.0f, 10.0f, 1.0f, BAPI_EASE_LINEAR, 1);
	expect(tween != NULL, "loop tween create");

	bapi_tween_manager_update(manager, 0.25f);
	expect(feq(bapi_tween_value(tween), 2.5f), "loop tween at 25%");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 7.5f), "loop tween at 75%");

	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(tween), 2.5f), "loop wraps back to 25%");
	expect(bapi_tween_is_finished(tween) == 0, "loop tween never finishes");

	/* A single large step wraps multiple times: 0.25 + 5.25 -> 5.5 -> 0.5. */
	bapi_tween_manager_update(manager, 5.25f);
	expect(feq(bapi_tween_value(tween), 5.0f), "loop wraps multiple durations");

	bapi_tween_manager_destroy(manager);
}

static void test_destroy(void)
{
	bapi_tween_manager_t manager = bapi_tween_manager_create();
	bapi_tween_t		 first;
	bapi_tween_t		 second;

	expect(manager != NULL, "manager create for destroy");
	first  = bapi_tween_create(manager, 0.0f, 1.0f, 1.0f, BAPI_EASE_LINEAR, 0);
	second = bapi_tween_create(manager, 0.0f, 2.0f, 1.0f, BAPI_EASE_LINEAR, 0);
	expect(first != NULL && second != NULL, "two tweens created");

	bapi_tween_destroy(manager, first);

	/* second must still be alive and advanced after first is removed. */
	bapi_tween_manager_update(manager, 0.5f);
	expect(feq(bapi_tween_value(second), 1.0f), "remaining tween still updates");

	bapi_tween_destroy(manager, second);
	bapi_tween_manager_update(manager, 1.0f);
	expect(1, "updating an empty manager is safe");
	bapi_tween_manager_destroy(manager);
}

static void test_invalid_args(void)
{
	bapi_tween_manager_t manager = bapi_tween_manager_create();
	bapi_tween_t		 tween;

	expect(manager != NULL, "manager create for invalid args");

	expect(bapi_tween_create(NULL, 0.0f, 1.0f, 1.0f, BAPI_EASE_LINEAR, 0) == NULL,
		   "create rejects NULL manager");
	expect(bapi_tween_create(manager, 0.0f, 1.0f, 0.0f, BAPI_EASE_LINEAR, 0) == NULL,
		   "create rejects zero duration");
	expect(bapi_tween_create(manager, 0.0f, 1.0f, -1.0f, BAPI_EASE_LINEAR, 0) == NULL,
		   "create rejects negative duration");

	tween = bapi_tween_create(manager, 0.0f, 1.0f, 1.0f, BAPI_EASE_LINEAR, 0);
	expect(tween != NULL, "valid tween create");

	/* Null handles are inert. */
	expect(feq(bapi_tween_value(NULL), 0.0f), "value(NULL) == 0");
	expect(bapi_tween_is_finished(NULL) == 1, "finished(NULL) == 1");
	bapi_tween_destroy(NULL, NULL);
	bapi_tween_destroy(manager, NULL);
	bapi_tween_manager_update(NULL, 0.5f);
	bapi_tween_manager_destroy(NULL);

	/* Negative delta time does not move the tween backwards. */
	bapi_tween_manager_update(manager, -1.0f);
	expect(feq(bapi_tween_value(tween), 0.0f), "negative delta ignored");

	bapi_tween_manager_destroy(manager);
}

int main(void)
{
	test_ease_endpoints();
	test_ease_mid_values();
	test_ease_clamp();
	test_out_back_overshoot();
	test_tween_linear_interpolation();
	test_tween_eased_value();
	test_loop_wrap();
	test_destroy();
	test_invalid_args();

	if (failures != 0) {
		fprintf(stderr, "%d tween test(s) failed\n", failures);
		return 1;
	}
	printf("all tween tests passed\n");
	return 0;
}
