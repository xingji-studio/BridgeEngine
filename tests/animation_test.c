/* Animation clip/state-machine tests. */
#include "BridgeEngine.h"

#include <stdio.h>
#include <string.h>

static int failures;

static int expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
	return condition;
}

static int rect_equal(bapi_rect_t a, bapi_rect_t b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static int test_add_and_play(void)
{
	static int		tex_a, tex_b;
	bapi_texture_t	texture		= (bapi_texture_t)(void *)&tex_a;
	bapi_texture_t	other		= (bapi_texture_t)(void *)&tex_b;
	bapi_animator_t animator	= bapi_animator_create();
	bapi_rect_t		frames[]	= {{0, 0, 16, 16}, {16, 0, 16, 16}, {32, 0, 16, 16}};
	float			durations[] = {0.1f, 0.1f, 0.1f};
	char			name[]		= "idle";

	expect(animator != NULL, "create returns animator");
	expect(bapi_animator_get_clip(animator)[0] == '\0', "no clip initially");
	expect(bapi_animator_get_texture(animator) == NULL, "no texture initially");
	expect(rect_equal(bapi_animator_get_frame(animator), (bapi_rect_t){0, 0, 0, 0}),
		   "no frame initially");
	expect(bapi_animator_is_finished(animator) == 1, "no active clip is finished");

	/* Validation failures. */
	expect(bapi_animator_add_clip(NULL, "x", texture, frames, durations, 3, 1) == -1,
		   "add_clip rejects NULL animator");
	expect(bapi_animator_add_clip(animator, NULL, texture, frames, durations, 3, 1) == -1,
		   "add_clip rejects NULL name");
	expect(bapi_animator_add_clip(animator, "", texture, frames, durations, 3, 1) == -1,
		   "add_clip rejects empty name");
	expect(bapi_animator_add_clip(animator, "bad", texture, NULL, durations, 3, 1) == -1,
		   "add_clip rejects NULL frames");
	expect(bapi_animator_add_clip(animator, "bad", texture, frames, NULL, 3, 1) == -1,
		   "add_clip rejects NULL durations");
	expect(bapi_animator_add_clip(animator, "bad", texture, frames, durations, 0, 1) == -1,
		   "add_clip rejects zero frame count");
	expect(bapi_animator_add_clip(animator, "bad", texture, frames, durations, -2, 1) == -1,
		   "add_clip rejects negative frame count");
	{
		float negative[] = {0.1f, -0.2f, 0.1f};
		expect(bapi_animator_add_clip(animator, "bad", texture, frames, negative, 3, 1) == -1,
			   "add_clip rejects negative duration");
	}

	/* Name and frame arrays are copied. */
	expect(bapi_animator_add_clip(animator, name, texture, frames, durations, 3, 1) == 0,
		   "add_clip succeeds");
	name[0]		= 'X';
	frames[0].x = 999.0f;
	expect(bapi_animator_play(animator, "idle") == 0, "play finds copied name");
	expect(strcmp(bapi_animator_get_clip(animator), "idle") == 0, "active clip name");
	expect(bapi_animator_get_texture(animator) == texture, "active clip texture");
	expect(rect_equal(bapi_animator_get_frame(animator), (bapi_rect_t){0, 0, 16, 16}),
		   "frame rect was copied");

	expect(bapi_animator_play(animator, "missing") == -1, "play missing clip fails");
	expect(strcmp(bapi_animator_get_clip(animator), "idle") == 0,
		   "failed play keeps previous clip");
	expect(bapi_animator_play(NULL, "idle") == -1, "play NULL animator fails");
	expect(bapi_animator_play(animator, NULL) == -1, "play NULL name fails");

	/* Duplicate name replaces the clip. */
	{
		bapi_rect_t replacement[]			= {{64, 0, 8, 8}, {72, 0, 8, 8}};
		float		replacement_durations[] = {0.5f, 0.5f};
		expect(bapi_animator_add_clip(animator, "idle", other, replacement, replacement_durations,
									  2, 0) == 0,
			   "duplicate add_clip replaces");
		expect(bapi_animator_get_texture(animator) == other, "replacement texture");
		expect(rect_equal(bapi_animator_get_frame(animator), (bapi_rect_t){64, 0, 8, 8}),
			   "replacement frame");
	}

	bapi_animator_destroy(animator);
	return 0;
}

static int test_frame_advancement(void)
{
	static int		tex;
	bapi_animator_t animator	= bapi_animator_create();
	bapi_rect_t		frames[]	= {{0, 0, 16, 16}, {16, 0, 16, 16}, {32, 0, 16, 16}};
	float			durations[] = {0.1f, 0.1f, 0.1f};

	bapi_animator_add_clip(animator, "walk", (bapi_texture_t)(void *)&tex, frames, durations, 3, 1);
	bapi_animator_play(animator, "walk");
	bapi_animator_update(animator, 0.05f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[0]), "still frame 0 mid-duration");

	bapi_animator_update(animator, 0.06f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[1]), "advanced to frame 1");

	bapi_animator_update(animator, 0.09f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[2]), "advanced to frame 2");

	bapi_animator_update(animator, -1.0f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[2]),
		   "negative delta does not advance");

	bapi_animator_destroy(animator);
	return 0;
}

static int test_looping(void)
{
	static int		tex;
	bapi_animator_t animator	= bapi_animator_create();
	bapi_rect_t		frames[]	= {{0, 0, 16, 16}, {16, 0, 16, 16}};
	float			durations[] = {0.1f, 0.1f};

	bapi_animator_add_clip(animator, "spin", (bapi_texture_t)(void *)&tex, frames, durations, 2, 1);
	bapi_animator_play(animator, "spin");
	bapi_animator_update(animator, 0.25f); /* frame0 -> frame1 -> frame0 + 0.05 */
	expect(rect_equal(bapi_animator_get_frame(animator), frames[0]), "loop wraps to frame 0");
	expect(bapi_animator_is_finished(animator) == 0, "looping clip never finishes");

	bapi_animator_update(animator, 0.06f); /* 0.05 + 0.06 crosses into frame1 */
	expect(rect_equal(bapi_animator_get_frame(animator), frames[1]), "loop continues to frame 1");

	bapi_animator_destroy(animator);
	return 0;
}

static int test_non_loop_finish(void)
{
	static int		tex;
	bapi_animator_t animator	= bapi_animator_create();
	bapi_rect_t		frames[]	= {{0, 0, 16, 16}, {16, 0, 16, 16}};
	float			durations[] = {0.1f, 0.1f};

	bapi_animator_add_clip(animator, "once", (bapi_texture_t)(void *)&tex, frames, durations, 2, 0);
	bapi_animator_play(animator, "once");
	expect(bapi_animator_is_finished(animator) == 0, "play clears finished");
	bapi_animator_update(animator, 0.1f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[1]), "reaches last frame");
	expect(bapi_animator_is_finished(animator) == 0, "not finished at start of last frame");

	bapi_animator_update(animator, 0.1f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[1]), "stays on last frame");
	expect(bapi_animator_is_finished(animator) == 1, "finished after last duration");

	bapi_animator_update(animator, 5.0f);
	expect(rect_equal(bapi_animator_get_frame(animator), frames[1]), "finished clip stays put");
	expect(bapi_animator_is_finished(animator) == 1, "stays finished");

	expect(bapi_animator_play(animator, "once") == 0, "replay finished clip");
	expect(rect_equal(bapi_animator_get_frame(animator), frames[0]), "play resets frame");
	expect(bapi_animator_is_finished(animator) == 0, "play resets finished state");

	bapi_animator_destroy(animator);
	return 0;
}

static int test_transition_on_bool(void)
{
	static int		tex;
	bapi_animator_t animator		 = bapi_animator_create();
	bapi_rect_t		idle_frames[]	 = {{0, 0, 16, 16}};
	bapi_rect_t		walk_frames[]	 = {{16, 0, 16, 16}, {32, 0, 16, 16}};
	float			idle_durations[] = {0.5f};
	float			walk_durations[] = {0.1f, 0.1f};

	bapi_animator_add_clip(animator, "idle", (bapi_texture_t)(void *)&tex, idle_frames,
						   idle_durations, 1, 1);
	bapi_animator_add_clip(animator, "walk", (bapi_texture_t)(void *)&tex, walk_frames,
						   walk_durations, 2, 1);
	expect(bapi_animator_add_transition(animator, "idle", "walk", "moving", 1) == 0,
		   "add_transition succeeds");
	expect(bapi_animator_add_transition(NULL, "idle", "walk", "moving", 1) == -1,
		   "add_transition rejects NULL animator");
	expect(bapi_animator_add_transition(animator, NULL, "walk", "moving", 1) == -1,
		   "add_transition rejects NULL from");
	expect(bapi_animator_add_transition(animator, "idle", NULL, "moving", 1) == -1,
		   "add_transition rejects NULL to");
	expect(bapi_animator_add_transition(animator, "idle", "walk", NULL, 1) == -1,
		   "add_transition rejects NULL parameter");

	bapi_animator_set_bool(animator, "moving", 0);
	bapi_animator_play(animator, "idle");
	bapi_animator_update(animator, 0.0f);
	expect(strcmp(bapi_animator_get_clip(animator), "idle") == 0,
		   "false parameter does not transition");

	bapi_animator_set_bool(animator, "moving", 1); /* also creates the parameter */
	bapi_animator_update(animator, 0.05f);
	expect(strcmp(bapi_animator_get_clip(animator), "walk") == 0, "bool transition switches clip");
	expect(rect_equal(bapi_animator_get_frame(animator), walk_frames[0]),
		   "transition restarts at frame 0 and consumes no time");

	bapi_animator_update(animator, 0.1f);
	expect(rect_equal(bapi_animator_get_frame(animator), walk_frames[1]),
		   "state advances after transition");

	/* set_bool updates an existing entry rather than duplicating it. */
	bapi_animator_set_bool(animator, "moving", 0);
	bapi_animator_play(animator, "idle");
	bapi_animator_update(animator, 0.0f);
	expect(strcmp(bapi_animator_get_clip(animator), "idle") == 0,
		   "updated bool disables transition");

	bapi_animator_destroy(animator);
	return 0;
}

static int test_transition_precedence(void)
{
	static int		tex;
	bapi_animator_t animator	  = bapi_animator_create();
	bapi_rect_t		idle_frames[] = {{0, 0, 16, 16}};
	bapi_rect_t		walk_frames[] = {{16, 0, 16, 16}};
	bapi_rect_t		run_frames[]  = {{32, 0, 16, 16}};
	float			one[]		  = {1.0f};

	bapi_animator_add_clip(animator, "idle", (bapi_texture_t)(void *)&tex, idle_frames, one, 1, 1);
	bapi_animator_add_clip(animator, "walk", (bapi_texture_t)(void *)&tex, walk_frames, one, 1, 1);
	bapi_animator_add_clip(animator, "run", (bapi_texture_t)(void *)&tex, run_frames, one, 1, 1);

	/* Both transitions match; insertion order decides, so "walk" must win. */
	expect(bapi_animator_add_transition(animator, "idle", "walk", "moving", 1) == 0,
		   "first transition added");
	expect(bapi_animator_add_transition(animator, "idle", "run", "moving", 1) == 0,
		   "second transition added");

	bapi_animator_set_bool(animator, "moving", 1);
	bapi_animator_play(animator, "idle");
	bapi_animator_update(animator, 0.0f);
	expect(strcmp(bapi_animator_get_clip(animator), "walk") == 0, "first matching transition wins");

	/* A transition whose `from` is not the current clip is skipped. */
	bapi_animator_add_transition(animator, "run", "idle", "stop", 1);
	bapi_animator_add_transition(animator, "walk", "idle", "stop", 1);
	bapi_animator_set_bool(animator, "stop", 1);
	bapi_animator_update(animator, 0.0f);
	expect(strcmp(bapi_animator_get_clip(animator), "idle") == 0,
		   "transition from non-matching state is skipped");

	/* Transition to a missing clip does not crash and does not switch. */
	bapi_animator_set_bool(animator, "stop", 0);
	bapi_animator_set_bool(animator, "missing", 1);
	bapi_animator_add_transition(animator, "walk", "ghost", "missing", 1);
	bapi_animator_play(animator, "walk");
	bapi_animator_update(animator, 0.0f);
	expect(strcmp(bapi_animator_get_clip(animator), "walk") == 0,
		   "transition to unknown clip leaves state unchanged");

	bapi_animator_destroy(animator);
	return 0;
}

static int test_destroy_null_and_lifetime(void)
{
	bapi_animator_destroy(NULL);
	expect(bapi_animator_get_clip(NULL)[0] == '\0', "get_clip NULL animator returns empty string");
	expect(bapi_animator_get_texture(NULL) == NULL, "get_texture NULL animator returns NULL");
	expect(rect_equal(bapi_animator_get_frame(NULL), (bapi_rect_t){0, 0, 0, 0}),
		   "get_frame NULL animator returns zero rect");
	expect(bapi_animator_is_finished(NULL) == 1, "is_finished NULL animator is finished");
	bapi_animator_update(NULL, 1.0f);
	bapi_animator_set_bool(NULL, "x", 1);
	return 0;
}

int main(void)
{
	test_add_and_play();
	test_frame_advancement();
	test_looping();
	test_non_loop_finish();
	test_transition_on_bool();
	test_transition_precedence();
	test_destroy_null_and_lifetime();

	if (failures != 0) {
		fprintf(stderr, "%d animation test assertion(s) failed\n", failures);
		return 1;
	}
	printf("animation tests passed\n");
	return 0;
}
