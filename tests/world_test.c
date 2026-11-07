#include "BridgeEngine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Entity drawing references the texture layer even when no sprite is set.
 * Provide tiny recording stubs so the test only needs world/entity/transform. */
static bapi_vec2_t g_draw_origin;
static int		   g_draw_count;

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

void bapi_texture_get_size(bapi_texture_t texture, int *w, int *h)
{
	(void)texture;
	if (w) *w = 10;
	if (h) *h = 10;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	(void)texture;
	(void)params;
	g_draw_origin = bapi_transform_point((bapi_vec2_t){0.0f, 0.0f});
	g_draw_count++;
}

static int g_order[64];
static int g_order_count;
static int g_counts[8];

static void recording_update(bapi_entity_t entity, float delta_time)
{
	(void)delta_time;
	int *id = (int *)bapi_entity_get_user_data(entity);
	if (id == NULL) return;
	g_counts[*id]++;
	if (g_order_count < (int)(sizeof(g_order) / sizeof(g_order[0]))) {
		g_order[g_order_count++] = *id;
	}
}

static bapi_world_t	 g_world;
static bapi_entity_t g_victim;
static int			 g_despawner_ran;
static int			 g_victim_alive_during_update;
static int			 g_count_during_update;

static void despawner_update(bapi_entity_t entity, float delta_time)
{
	(void)delta_time;
	int *id = (int *)bapi_entity_get_user_data(entity);
	if (id != NULL) {
		g_counts[*id]++;
		g_order[g_order_count++] = *id;
	}

	if (!g_despawner_ran) {
		g_despawner_ran = 1;
		/* Despawn during update is deferred: the entity must stay readable and
		 * the request must be tolerated twice. */
		bapi_world_despawn(g_world, g_victim);
		bapi_world_despawn(g_world, g_victim);
		g_victim_alive_during_update = bapi_entity_get_name(g_victim) != NULL &&
									   strcmp(bapi_entity_get_name(g_victim), "two") == 0;
		g_count_during_update		 = bapi_world_entity_count(g_world);
	}
}

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static int nearly_equal(float a, float b)
{
	return fabsf(a - b) < 0.001f;
}

static int test_world_create_and_spawn(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	CHECK(bapi_world_entity_count(world) == 0);
	CHECK(bapi_world_get_entity(world, 0) == NULL);
	CHECK(bapi_world_get_entity(world, -1) == NULL);

	static int	  ids[3];
	bapi_entity_t first	 = bapi_world_spawn(world, "one");
	bapi_entity_t second = bapi_world_spawn(world, "two");
	bapi_entity_t third	 = bapi_world_spawn(world, "three");
	CHECK(first != NULL && second != NULL && third != NULL);
	CHECK(bapi_world_entity_count(world) == 3);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 0)), "one") == 0);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 1)), "two") == 0);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 2)), "three") == 0);
	CHECK(bapi_world_get_entity(world, 3) == NULL);

	/* Spawned entities are roots. */
	CHECK(bapi_entity_get_parent(first) == NULL);

	/* Unnamed spawn gets the entity default name. */
	bapi_entity_t unnamed = bapi_world_spawn(world, NULL);
	CHECK(unnamed != NULL);
	CHECK(bapi_world_entity_count(world) == 4);
	CHECK(strcmp(bapi_entity_get_name(unnamed), "entity") == 0);
	bapi_world_despawn(world, unnamed);
	CHECK(bapi_world_entity_count(world) == 3);

	ids[0] = 0;
	ids[1] = 1;
	ids[2] = 2;
	bapi_entity_set_user_data(first, &ids[0]);
	bapi_entity_set_user_data(second, &ids[1]);
	bapi_entity_set_user_data(third, &ids[2]);
	bapi_entity_set_update(first, recording_update);
	bapi_entity_set_update(second, recording_update);
	bapi_entity_set_update(third, recording_update);

	/* Camera roundtrip. */
	bapi_camera_t camera = {10.0f, 20.0f, 2.5f, 320.0f, 240.0f, 0.5f};
	bapi_world_set_camera(world, camera);
	bapi_camera_t readback;
	bapi_world_get_camera(world, &readback);
	CHECK(nearly_equal(readback.x, 10.0f));
	CHECK(nearly_equal(readback.y, 20.0f));
	CHECK(nearly_equal(readback.zoom, 2.5f));
	CHECK(nearly_equal(readback.viewport_w, 320.0f));
	CHECK(nearly_equal(readback.viewport_h, 240.0f));
	CHECK(nearly_equal(readback.rotation, 0.5f));

	/* Pause skips callbacks entirely. */
	g_order_count = 0;
	memset(g_counts, 0, sizeof(g_counts));
	bapi_world_set_paused(world, 1);
	CHECK(bapi_world_is_paused(world) == 1);
	bapi_world_update(world, 0.016f);
	CHECK(g_order_count == 0);
	bapi_world_set_paused(world, 0);
	CHECK(bapi_world_is_paused(world) == 0);

	/* Update runs roots in insertion order. */
	bapi_world_update(world, 0.016f);
	CHECK(g_order_count == 3);
	CHECK(g_order[0] == 0 && g_order[1] == 1 && g_order[2] == 2);
	CHECK(g_counts[0] == 1 && g_counts[1] == 1 && g_counts[2] == 1);

	bapi_world_destroy(world);
	bapi_world_destroy(NULL);
	return 0;
}

static int test_deferred_despawn(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	g_world = world;

	static int ids[3];
	ids[0] = 0;
	ids[1] = 1;
	ids[2] = 2;

	bapi_entity_t first	 = bapi_world_spawn(world, "one");
	bapi_entity_t second = bapi_world_spawn(world, "two");
	bapi_entity_t third	 = bapi_world_spawn(world, "three");
	CHECK(first != NULL && second != NULL && third != NULL);
	bapi_entity_set_user_data(first, &ids[0]);
	bapi_entity_set_user_data(second, &ids[1]);
	bapi_entity_set_user_data(third, &ids[2]);
	bapi_entity_set_update(first, despawner_update);
	bapi_entity_set_update(second, recording_update);
	bapi_entity_set_update(third, recording_update);

	g_victim					 = second;
	g_despawner_ran				 = 0;
	g_victim_alive_during_update = 0;
	g_count_during_update		 = -1;
	g_order_count				 = 0;
	memset(g_counts, 0, sizeof(g_counts));

	bapi_world_update(world, 0.016f);

	CHECK(g_despawner_ran == 1);
	CHECK(g_victim_alive_during_update == 1);
	CHECK(g_count_during_update == 2);
	/* Victim was removed from the root list before its snapshot turn, so its
	 * callback did not run. */
	CHECK(g_counts[1] == 0);
	CHECK(g_order_count == 2);
	CHECK(g_order[0] == 0 && g_order[1] == 2);
	CHECK(bapi_world_entity_count(world) == 2);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 0)), "one") == 0);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 1)), "three") == 0);

	/* Deferred destruction happened at the end of the update; the victim never
	 * updates again. */
	g_order_count = 0;
	bapi_world_update(world, 0.016f);
	CHECK(g_counts[1] == 0);
	CHECK(g_order_count == 2);
	CHECK(g_order[0] == 0 && g_order[1] == 2);

	bapi_world_destroy(world);
	return 0;
}

static int test_child_and_subtree_despawn(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);

	bapi_entity_t parent = bapi_world_spawn(world, "parent");
	CHECK(parent != NULL);
	CHECK(bapi_world_entity_count(world) == 1);

	/* Despawning a child destroys just that child and detaches it. */
	bapi_entity_t child = bapi_entity_create("child");
	CHECK(child != NULL);
	CHECK(bapi_entity_add_child(parent, child) == 0);
	CHECK(bapi_entity_get_child_count(parent) == 1);
	bapi_world_despawn(world, child);
	CHECK(bapi_entity_get_child_count(parent) == 0);
	CHECK(bapi_world_entity_count(world) == 1);

	/* Despawning a root owns its subtree: no double destruction. */
	bapi_entity_t child_a = bapi_entity_create("child_a");
	bapi_entity_t child_b = bapi_entity_create("child_b");
	CHECK(child_a != NULL && child_b != NULL);
	CHECK(bapi_entity_add_child(parent, child_a) == 0);
	CHECK(bapi_entity_add_child(child_a, child_b) == 0);
	bapi_world_despawn(world, parent);
	CHECK(bapi_world_entity_count(world) == 0);

	bapi_world_destroy(world);
	return 0;
}

static int test_draw_camera_and_reset(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);

	bapi_entity_t sprite = bapi_world_spawn(world, "sprite");
	CHECK(sprite != NULL);
	bapi_entity_set_position(sprite, (bapi_vec2_t){150.0f, 0.0f});
	bapi_entity_set_texture(sprite, (bapi_texture_t)(void *)&g_draw_count,
							(bapi_rect_t){0.0f, 0.0f, 10.0f, 10.0f});

	bapi_camera_t camera = {100.0f, 0.0f, 2.0f, 200.0f, 100.0f, 0.0f};
	bapi_world_set_camera(world, camera);

	g_draw_count  = 0;
	g_draw_origin = (bapi_vec2_t){-1.0f, -1.0f};
	bapi_world_draw(world);

	CHECK(g_draw_count == 1);
	/* (150 - 100) * 2 + 200 / 2 = 200; 0 * 2 + 100 / 2 = 50 */
	CHECK(nearly_equal(g_draw_origin.x, 200.0f));
	CHECK(nearly_equal(g_draw_origin.y, 50.0f));

	/* The world resets the transform stack after drawing so caller state is
	 * not polluted by the camera matrix. */
	bapi_vec2_t point = bapi_transform_point((bapi_vec2_t){7.0f, 9.0f});
	CHECK(nearly_equal(point.x, 7.0f));
	CHECK(nearly_equal(point.y, 9.0f));

	/* Camera is untouched by drawing. */
	bapi_camera_t readback;
	bapi_world_get_camera(world, &readback);
	CHECK(nearly_equal(readback.x, 100.0f));
	CHECK(nearly_equal(readback.zoom, 2.0f));

	bapi_world_destroy(world);
	return 0;
}

int main(void)
{
	if (test_world_create_and_spawn() != 0) return 1;
	if (test_deferred_despawn() != 0) return 1;
	if (test_child_and_subtree_despawn() != 0) return 1;
	if (test_draw_camera_and_reset() != 0) return 1;

	printf("world_test: ok\n");
	return 0;
}
