#include "BridgeEngine.h"

#include <stdio.h>
#include <string.h>

void bapi_texture_get_size(bapi_texture_t texture, int *w, int *h)
{
	(void)texture;
	if (w) *w = 16;
	if (h) *h = 16;
}

bapi_texture_draw_t bapi_texture_draw_default(bapi_rect_t dst)
{
	bapi_texture_draw_t params;
	memset(&params, 0, sizeof(params));
	params.dst	   = dst;
	params.pivot_x = 0.5f;
	params.pivot_y = 0.5f;
	params.tint	   = (bapi_color_t){255, 255, 255, 255};
	return params;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	(void)texture;
	(void)params;
}

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)color;
}

static int failures;

#define CHECK(condition, message)                                                                  \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__);                            \
			failures++;                                                                            \
		}                                                                                          \
	} while (0)

int main(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL, "world create");

	bapi_entity_t player = bapi_world_spawn(world, "player");
	CHECK(player != NULL, "world spawn");
	bapi_entity_set_position(player, (bapi_vec2_t){40.0f, 40.0f});
	CHECK(bapi_world_entity_count(world) == 1, "world count");
	CHECK(bapi_world_get_entity(world, 0) == player, "world root");

	bapi_tilemap_t map = bapi_tilemap_create(10, 10, 16, 16);
	CHECK(map != NULL, "tilemap create");
	for (int x = 0; x < 10; x++) bapi_tilemap_set_tile(map, x, 5, 1, 1);
	CHECK(bapi_tilemap_is_solid(map, 5, 5), "solid tile");
	CHECK(!bapi_tilemap_is_solid(map, 5, 4), "empty tile");

	bapi_rect_t body	= {32.0f, 72.0f, 16.0f, 16.0f};
	int			blocked = bapi_tilemap_move_rect(map, &body, (bapi_vec2_t){0.0f, 40.0f});
	CHECK(blocked == 1, "downward collision blocks");
	CHECK(body.y <= 80.0f, "player stopped above wall");

	bapi_rect_t free_body = {32.0f, 32.0f, 16.0f, 16.0f};
	blocked				  = bapi_tilemap_move_rect(map, &free_body, (bapi_vec2_t){20.0f, 0.0f});
	CHECK(blocked == 0, "open horizontal movement");
	CHECK(free_body.x == 52.0f, "horizontal movement applied");

	bapi_texture_t fake_texture	 = (bapi_texture_t)(size_t)1;
	bapi_rect_t	   idle_frame	 = {0, 0, 16, 16};
	bapi_rect_t	   run_frames[2] = {{0, 0, 16, 16}, {16, 0, 16, 16}};
	float		   idle_time	 = 1.0f;
	float		   run_times[2]	 = {0.1f, 0.1f};

	bapi_animator_t animator = bapi_animator_create();
	CHECK(animator != NULL, "animator create");
	CHECK(bapi_animator_add_clip(animator, "idle", fake_texture, &idle_frame, &idle_time, 1, 1) ==
			  0,
		  "idle clip");
	CHECK(bapi_animator_add_clip(animator, "run", fake_texture, run_frames, run_times, 2, 1) == 0,
		  "run clip");
	CHECK(bapi_animator_add_transition(animator, "idle", "run", "moving", 1) == 0,
		  "idle->run transition");
	CHECK(bapi_animator_add_transition(animator, "run", "idle", "moving", 0) == 0,
		  "run->idle transition");
	CHECK(bapi_animator_play(animator, "idle") == 0, "play idle");

	bapi_animator_set_bool(animator, "moving", 1);
	bapi_animator_update(animator, 1.0f / 60.0f);
	CHECK(bapi_animator_get_clip(animator) != NULL && bapi_animator_get_clip(animator)[0] == 'r',
		  "moving triggers run");

	bapi_animator_update(animator, 0.11f);
	bapi_rect_t frame = bapi_animator_get_frame(animator);
	CHECK(frame.x == 16.0f, "run animation advanced");

	bapi_animator_set_bool(animator, "moving", 0);
	bapi_animator_update(animator, 0.0f);
	CHECK(bapi_animator_get_clip(animator)[0] == 'i', "stop triggers idle");

	bapi_animator_destroy(animator);
	bapi_tilemap_destroy(map);
	bapi_world_destroy(world);

	if (failures == 0) printf("vertical slice integration: ok\n");
	return failures == 0 ? 0 : 1;
}
