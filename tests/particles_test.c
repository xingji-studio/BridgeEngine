/* Particle system tests. */

#include "BridgeEngine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_CAPTURED 32

typedef struct {
	float		 x;
	float		 y;
	float		 w;
	float		 h;
	bapi_color_t color;
} captured_rect_t;

static captured_rect_t g_rects[MAX_CAPTURED];
static int			   g_rect_count;
static int			   g_failures;

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	if (g_rect_count < MAX_CAPTURED) {
		g_rects[g_rect_count].x		= x;
		g_rects[g_rect_count].y		= y;
		g_rects[g_rect_count].w		= w;
		g_rects[g_rect_count].h		= h;
		g_rects[g_rect_count].color = color;
	}
	g_rect_count++;
}

static void capture_reset(void)
{
	g_rect_count = 0;
}

static int expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		g_failures++;
	}
	return condition;
}

static int float_equal(float a, float b)
{
	return fabsf(a - b) < 0.0005f;
}

static int color_equal(bapi_color_t a, bapi_color_t b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bapi_particle_config_t make_config(void)
{
	bapi_particle_config_t config;

	memset(&config, 0, sizeof(config));
	config.color_start = (bapi_color_t){255, 255, 255, 255};
	config.color_end   = (bapi_color_t){255, 255, 255, 255};
	config.life_min	   = 1.0f;
	config.life_max	   = 1.0f;
	config.size_min	   = 4.0f;
	config.size_max	   = 4.0f;
	return config;
}

static int test_create_and_null(void)
{
	bapi_particle_system_t system;

	expect(bapi_particle_system_create(0) == NULL, "create rejects zero capacity");
	expect(bapi_particle_system_create(-3) == NULL, "create rejects negative capacity");

	system = bapi_particle_system_create(4);
	expect(system != NULL, "create accepts positive capacity");
	expect(bapi_particle_system_count(system) == 0, "new system is empty");

	/* Defaults: one white 2x2 particle, 1s life, no motion. */
	bapi_particle_system_emit(system, 1);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rect_count == 1, "default config draws one rect");
	expect(float_equal(g_rects[0].w, 2.0f) && float_equal(g_rects[0].h, 2.0f),
		   "default size is 2x2");
	expect(color_equal(g_rects[0].color, (bapi_color_t){255, 255, 255, 255}),
		   "default colour is opaque white");

	bapi_particle_system_destroy(system);

	/* NULL is a no-op for every entry point. */
	bapi_particle_system_destroy(NULL);
	bapi_particle_system_set_config(NULL, NULL);
	bapi_particle_system_set_position(NULL, 1.0f, 2.0f);
	bapi_particle_system_emit(NULL, 5);
	bapi_particle_system_update(NULL, 1.0f);
	bapi_particle_system_draw(NULL);
	expect(bapi_particle_system_count(NULL) == 0, "count NULL is zero");
	return 0;
}

static int test_emit_and_capacity(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(3);
	bapi_particle_config_t config = make_config();

	bapi_particle_system_set_config(system, &config);
	expect(bapi_particle_system_count(system) == 0, "emit target starts empty");

	bapi_particle_system_emit(system, 2);
	expect(bapi_particle_system_count(system) == 2, "emit adds requested count");

	bapi_particle_system_emit(system, 5);
	expect(bapi_particle_system_count(system) == 3, "emit clamps at capacity");

	bapi_particle_system_emit(system, 0);
	bapi_particle_system_emit(system, -2);
	expect(bapi_particle_system_count(system) == 3, "emit ignores non-positive counts");

	/* All live particles draw in emission/pool order. */
	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rect_count == 3, "draw visits every live particle");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_position_and_update_motion(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(4);
	bapi_particle_config_t config = make_config();

	config.velocity_min = (bapi_vec2_t){10.0f, 0.0f};
	config.velocity_max = (bapi_vec2_t){10.0f, 0.0f};
	config.gravity		= 2.0f;
	config.life_min		= 10.0f;
	config.life_max		= 10.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_set_position(system, 100.0f, 50.0f);
	bapi_particle_system_emit(system, 1);

	capture_reset();
	bapi_particle_system_draw(system);
	expect(float_equal(g_rects[0].x, 100.0f) && float_equal(g_rects[0].y, 50.0f),
		   "emit uses emitter position");

	/* semi-implicit: vy += g*dt, then pos += v*dt */
	bapi_particle_system_update(system, 0.5f);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(float_equal(g_rects[0].x, 105.0f), "gravity has no effect on x");
	expect(float_equal(g_rects[0].y, 50.5f), "velocity integrates with gravity");

	bapi_particle_system_update(system, -1.0f);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(float_equal(g_rects[0].x, 105.0f) && float_equal(g_rects[0].y, 50.5f),
		   "negative dt does not move particles");

	bapi_particle_system_set_position(system, -5.0f, -6.0f);
	bapi_particle_system_emit(system, 1);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(float_equal(g_rects[1].x, -5.0f) && float_equal(g_rects[1].y, -6.0f),
		   "set_position moves the emitter origin for later emits");
	expect(float_equal(g_rects[0].x, 105.0f), "set_position does not move live particles");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_age_and_death(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(8);
	bapi_particle_config_t config = make_config();

	config.life_min = 0.25f;
	config.life_max = 0.25f;
	bapi_particle_system_set_config(system, &config);

	bapi_particle_system_emit(system, 4);
	expect(bapi_particle_system_count(system) == 4, "four particles spawned");

	bapi_particle_system_update(system, 0.24f);
	expect(bapi_particle_system_count(system) == 4, "particles survive within their life");

	bapi_particle_system_update(system, 0.01f);
	expect(bapi_particle_system_count(system) == 0, "particles die once age reaches life");

	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rect_count == 0, "dead particles are not drawn");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_emission_rate(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(16);
	bapi_particle_config_t config = make_config();

	config.life_min		 = 100.0f;
	config.life_max		 = 100.0f;
	config.emission_rate = 10.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_set_position(system, 3.0f, 4.0f);

	bapi_particle_system_update(system, 0.25f);
	expect(bapi_particle_system_count(system) == 2, "emission rate spawns whole particles");

	bapi_particle_system_update(system, 0.05f);
	expect(bapi_particle_system_count(system) == 3, "emission accumulator carries the fraction");

	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rect_count == 3, "emitted particles are drawable");
	expect(float_equal(g_rects[0].x, 3.0f) && float_equal(g_rects[0].y, 4.0f),
		   "emitted particles start at the emitter position with age 0");

	/* emission_rate == 0 spawns nothing */
	config.emission_rate = 0.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_update(system, 1.0f);
	expect(bapi_particle_system_count(system) == 3, "zero emission rate spawns nothing");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_color_interpolation(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(4);
	bapi_particle_config_t config = make_config();

	config.color_start = (bapi_color_t){255, 0, 0, 255};
	config.color_end   = (bapi_color_t){0, 0, 255, 0};
	config.life_min	   = 1.0f;
	config.life_max	   = 1.0f;
	config.size_min	   = 10.0f;
	config.size_max	   = 10.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_emit(system, 1);

	capture_reset();
	bapi_particle_system_draw(system);
	expect(color_equal(g_rects[0].color, (bapi_color_t){255, 0, 0, 255}), "age 0 uses color_start");
	expect(float_equal(g_rects[0].w, 10.0f), "particle size is applied");

	bapi_particle_system_update(system, 0.5f);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(color_equal(g_rects[0].color, (bapi_color_t){128, 0, 128, 128}),
		   "half life blends color_start/color_end");

	bapi_particle_system_update(system, 0.49f);
	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rects[0].color.r < 10 && g_rects[0].color.b > 245,
		   "near-end-of-life approaches color_end");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_draw_order(void)
{
	bapi_particle_system_t system = bapi_particle_system_create(8);
	bapi_particle_config_t config = make_config();

	bapi_particle_system_set_position(system, 7.0f, 8.0f);
	config.size_min = 2.0f;
	config.size_max = 2.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_emit(system, 1);

	config.size_min = 6.0f;
	config.size_max = 6.0f;
	bapi_particle_system_set_config(system, &config);
	bapi_particle_system_emit(system, 1);

	capture_reset();
	bapi_particle_system_draw(system);
	expect(g_rect_count == 2, "two rects drawn");
	expect(float_equal(g_rects[0].w, 2.0f) && float_equal(g_rects[0].h, 2.0f),
		   "first emitted particle draws first with its captured size");
	expect(float_equal(g_rects[1].w, 6.0f) && float_equal(g_rects[1].h, 6.0f),
		   "second emitted particle draws second with its captured size");

	bapi_particle_system_destroy(system);
	return 0;
}

static int test_determinism(void)
{
	bapi_particle_system_t a	  = bapi_particle_system_create(8);
	bapi_particle_system_t b	  = bapi_particle_system_create(8);
	bapi_particle_config_t config = make_config();

	config.velocity_min = (bapi_vec2_t){-5.0f, -5.0f};
	config.velocity_max = (bapi_vec2_t){5.0f, 5.0f};
	config.life_min		= 0.5f;
	config.life_max		= 2.0f;
	config.size_min		= 1.0f;
	config.size_max		= 4.0f;
	bapi_particle_system_set_config(a, &config);
	bapi_particle_system_set_config(b, &config);
	bapi_particle_system_emit(a, 4);
	bapi_particle_system_emit(b, 4);
	bapi_particle_system_update(a, 0.3f);
	bapi_particle_system_update(b, 0.3f);

	capture_reset();
	bapi_particle_system_draw(a);
	expect(g_rect_count == 4, "system A draws all particles");
	{
		captured_rect_t first[MAX_CAPTURED];
		int				count = g_rect_count;
		int				i;

		memcpy(first, g_rects, sizeof(first));
		capture_reset();
		bapi_particle_system_draw(b);
		expect(g_rect_count == count, "system B draws the same count");
		for (i = 0; i < count; i++) {
			expect(float_equal(first[i].x, g_rects[i].x) && float_equal(first[i].y, g_rects[i].y) &&
					   float_equal(first[i].w, g_rects[i].w),
				   "fake-random emit is deterministic across systems");
		}
	}

	bapi_particle_system_destroy(a);
	bapi_particle_system_destroy(b);
	return 0;
}

int main(void)
{
	test_create_and_null();
	test_emit_and_capacity();
	test_position_and_update_motion();
	test_age_and_death();
	test_emission_rate();
	test_color_interpolation();
	test_draw_order();
	test_determinism();

	if (g_failures != 0) {
		fprintf(stderr, "%d particle test assertion(s) failed\n", g_failures);
		return 1;
	}
	printf("particle tests passed\n");
	return 0;
}
