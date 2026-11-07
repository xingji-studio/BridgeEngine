#include "BridgeEngine.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                                                \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
			g_failures++;                                                                          \
		}                                                                                          \
	} while (0)

static int near(float a, float b, float eps)
{
	float d = a - b;
	if (d < 0.0f) d = -d;
	return d <= eps;
}

static void test_create_destroy(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam;

	CHECK(c != NULL);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 0.0f, 1e-6f));
	CHECK(near(cam.y, 0.0f, 1e-6f));
	CHECK(near(cam.zoom, 1.0f, 1e-6f));
	CHECK(near(cam.rotation, 0.0f, 1e-6f));
	CHECK(near(cam.viewport_w, 320.0f, 1e-6f));
	CHECK(near(cam.viewport_h, 240.0f, 1e-6f));

	bapi_camera_controller_destroy(c);
	bapi_camera_controller_destroy(NULL);
	bapi_camera_controller_get_camera(NULL, &cam);
	CHECK(near(cam.viewport_w, 0.0f, 1e-6f));
}

static void test_instant_follow(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam;

	/* smoothing == 0 means instant positioning. */
	bapi_camera_controller_update(c, 1.0f / 60.0f, 100.0f, 50.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 100.0f, 1e-5f));
	CHECK(near(cam.y, 50.0f, 1e-5f));

	bapi_camera_controller_update(c, 1.0f / 60.0f, -20.0f, 15.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, -20.0f, 1e-5f));
	CHECK(near(cam.y, 15.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

static void test_deadzone(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam;

	bapi_camera_controller_set_deadzone(c, 40.0f, 30.0f);

	/* Inside +/- half extents (20, 15): camera holds. */
	bapi_camera_controller_update(c, 0.1f, 15.0f, 10.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 0.0f, 1e-5f));
	CHECK(near(cam.y, 0.0f, 1e-5f));

	/* Just outside: camera catches up by exactly the overhang (instant). */
	bapi_camera_controller_update(c, 0.1f, 100.0f, 100.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 80.0f, 1e-5f));
	CHECK(near(cam.y, 85.0f, 1e-5f));

	/* Moving back inside the new deadzone leaves the camera unchanged. */
	bapi_camera_controller_update(c, 0.1f, 90.0f, 90.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 80.0f, 1e-5f));
	CHECK(near(cam.y, 85.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

static void test_smoothing_converges(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam;
	int						 i;

	bapi_camera_controller_set_smoothing(c, 4.0f);

	/* First step with dt=0.1 -> factor 0.4. */
	bapi_camera_controller_update(c, 0.1f, 100.0f, 0.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 40.0f, 1e-4f));

	for (i = 0; i < 200; i++) {
		float prev = cam.x;
		bapi_camera_controller_update(c, 1.0f / 60.0f, 100.0f, 0.0f);
		bapi_camera_controller_get_camera(c, &cam);
		CHECK(cam.x >= prev - 1e-6f); /* monotonic approach */
	}
	CHECK(near(cam.x, 100.0f, 1e-3f));

	/* Large dt is clamped to a full step, never overshoots. */
	bapi_camera_controller_update(c, 1000.0f, 100.0f, 0.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 100.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

static void test_bounds_clamp(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(100.0f, 50.0f);
	bapi_camera_t			 cam;
	bapi_rect_t				 bounds = {0.0f, 0.0f, 200.0f, 100.0f};

	bapi_camera_controller_set_bounds(c, bounds);

	bapi_camera_controller_update(c, 0.1f, -1000.0f, -1000.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 50.0f, 1e-5f));
	CHECK(near(cam.y, 25.0f, 1e-5f));

	bapi_camera_controller_update(c, 0.1f, 1000.0f, 1000.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 150.0f, 1e-5f));
	CHECK(near(cam.y, 75.0f, 1e-5f));

	/* Bounds smaller than the viewport center the camera. */
	bapi_camera_controller_set_bounds(c, (bapi_rect_t){0.0f, 0.0f, 40.0f, 30.0f});
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 20.0f, 1e-5f));
	CHECK(near(cam.y, 15.0f, 1e-5f));

	/* width/height <= 0 disables the bounds again. */
	bapi_camera_controller_set_bounds(c, (bapi_rect_t){0.0f, 0.0f, 0.0f, 0.0f});
	bapi_camera_controller_update(c, 0.1f, 500.0f, 500.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 500.0f, 1e-5f));
	CHECK(near(cam.y, 500.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

static void test_shake_decays(void)
{
	bapi_camera_controller_t c1 = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_controller_t c2 = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam1;
	bapi_camera_t			 cam2;
	int						 i;

	bapi_camera_controller_shake(c1, 10.0f, 0.1f);
	bapi_camera_controller_shake(c2, 10.0f, 0.1f);

	/* Halfway through: offset exists and is bounded by the amplitude. */
	bapi_camera_controller_update(c1, 0.05f, 0.0f, 0.0f);
	bapi_camera_controller_update(c2, 0.05f, 0.0f, 0.0f);
	bapi_camera_controller_get_camera(c1, &cam1);
	bapi_camera_controller_get_camera(c2, &cam2);
	CHECK(cam1.x != 0.0f || cam1.y != 0.0f);
	CHECK(near(cam1.x, cam2.x, 1e-6f)); /* deterministic */
	CHECK(near(cam1.y, cam2.y, 1e-6f));

	/* After the duration the offset is exactly zero. */
	bapi_camera_controller_update(c1, 0.05f, 0.0f, 0.0f);
	bapi_camera_controller_get_camera(c1, &cam1);
	CHECK(near(cam1.x, 0.0f, 1e-6f));
	CHECK(near(cam1.y, 0.0f, 1e-6f));

	/* Repeated updates keep it at zero. */
	for (i = 0; i < 20; i++) bapi_camera_controller_update(c1, 0.01f, 0.0f, 0.0f);
	bapi_camera_controller_get_camera(c1, &cam1);
	CHECK(near(cam1.x, 0.0f, 1e-6f));
	CHECK(near(cam1.y, 0.0f, 1e-6f));

	/* Each sample stays inside the linearly decaying amplitude envelope. */
	bapi_camera_controller_shake(c1, 8.0f, 0.2f);
	{
		float prev_bound = 8.0f;
		for (i = 0; i < 20; i++) {
			float dx;
			float dy;
			float mag;
			float bound = 8.0f * (0.2f - 0.01f * (float)(i + 1)) / 0.2f;
			bapi_camera_controller_update(c1, 0.01f, 0.0f, 0.0f);
			bapi_camera_controller_get_camera(c1, &cam1);
			dx	= cam1.x < 0.0f ? -cam1.x : cam1.x;
			dy	= cam1.y < 0.0f ? -cam1.y : cam1.y;
			mag = dx > dy ? dx : dy;
			CHECK(mag <= bound + 1e-4f);
			CHECK(bound <= prev_bound + 1e-6f);
			prev_bound = bound;
		}
		CHECK(near(prev_bound, 0.0f, 1e-6f));
	}

	bapi_camera_controller_destroy(c1);
	bapi_camera_controller_destroy(c2);
}

static void test_snap_and_follow(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(320.0f, 240.0f);
	bapi_camera_t			 cam;

	bapi_camera_controller_set_smoothing(c, 2.0f);
	bapi_camera_controller_follow(c, 50.0f, 60.0f);

	/* follow() alone does not move the camera. */
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 0.0f, 1e-5f));
	CHECK(near(cam.y, 0.0f, 1e-5f));

	/* snap() jumps directly to the followed target. */
	bapi_camera_controller_snap(c);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 50.0f, 1e-5f));
	CHECK(near(cam.y, 60.0f, 1e-5f));

	/* NaN update args reuse the recorded target. */
	bapi_camera_controller_snap(c);
	bapi_camera_controller_update(c, 0.1f, NAN, NAN);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 50.0f, 1e-5f));
	CHECK(near(cam.y, 60.0f, 1e-5f));

	/* Non-NaN update args take precedence and become the new follow target. */
	bapi_camera_controller_update(c, 1.0f, 10.0f, 20.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.x, 10.0f, 1e-5f));
	CHECK(near(cam.y, 20.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

static void test_viewport_and_get_camera(void)
{
	bapi_camera_controller_t c = bapi_camera_controller_create(100.0f, 100.0f);
	bapi_camera_t			 cam;

	bapi_camera_controller_set_viewport(c, 800.0f, 600.0f);
	bapi_camera_controller_update(c, 0.1f, 10.0f, 20.0f);
	bapi_camera_controller_get_camera(c, &cam);
	CHECK(near(cam.viewport_w, 800.0f, 1e-5f));
	CHECK(near(cam.viewport_h, 600.0f, 1e-5f));
	CHECK(near(cam.zoom, 1.0f, 1e-6f));
	CHECK(near(cam.rotation, 0.0f, 1e-6f));
	CHECK(near(cam.x, 10.0f, 1e-5f));
	CHECK(near(cam.y, 20.0f, 1e-5f));

	bapi_camera_controller_destroy(c);
}

int main(void)
{
	test_create_destroy();
	test_instant_follow();
	test_deadzone();
	test_smoothing_converges();
	test_bounds_clamp();
	test_shake_decays();
	test_snap_and_follow();
	test_viewport_and_get_camera();

	if (g_failures != 0) {
		printf("%d camera controller check(s) failed\n", g_failures);
		return 1;
	}
	printf("camera controller tests passed\n");
	return 0;
}
