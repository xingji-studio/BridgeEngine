#include "BridgeEngine.h"

#include <stdio.h>

/* AABB collision world tests. */

#define CHECK_TOLERANCE 1e-3f
#define EPS_GAP			1e-4f

static int g_checks;
static int g_failures;

static void check(int condition, const char *message)
{
	g_checks++;
	if (!condition) {
		g_failures++;
		fprintf(stderr, "FAIL: %s\n", message);
	}
}

static int nearly_equal(float a, float b, float tolerance)
{
	float diff = a - b;
	if (diff < 0.0f) diff = -diff;
	return diff <= tolerance;
}

static bapi_rect_t make_rect(float x, float y, float w, float h)
{
	bapi_rect_t rect = {x, y, w, h};
	return rect;
}

static int contains_handle(bapi_collider_t *handles, int count, bapi_collider_t value)
{
	for (int i = 0; i < count; i++) {
		if (handles[i] == value) return 1;
	}
	return 0;
}

static void test_add_remove_query(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "world_create returns a world");
	if (world == NULL) return;

	bapi_collider_t first =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	bapi_collider_t second =
		bapi_collision_world_add(world, make_rect(30, 0, 10, 10), 2, 0xFFFFFFFFu, 1);
	bapi_collider_t third =
		bapi_collision_world_add(world, make_rect(60, 0, 10, 10), 4, 0xFFFFFFFFu, 0);
	check(first != NULL && second != NULL && third != NULL, "add returns handles");
	check(first != second && second != third && first != third, "handles are distinct");

	bapi_collider_t out[8] = {0};
	int found = bapi_collision_world_query(world, make_rect(-100, -100, 1000, 1000), 0, out, 8);
	check(found == 3, "query counts every collider");
	check(contains_handle(out, 3, first) && contains_handle(out, 3, second) &&
			  contains_handle(out, 3, third),
		  "query reports every handle");

	found = bapi_collision_world_query(world, make_rect(-100, -100, 1000, 1000), 0, out, 2);
	check(found == 3, "query returns total found beyond max");
	check(out[0] == first && out[1] == second, "query fills up to max in insertion order");

	bapi_collision_world_remove(world, second);
	found = bapi_collision_world_query(world, make_rect(-100, -100, 1000, 1000), 0, out, 8);
	check(found == 2, "remove lowers the collider count");
	check(!contains_handle(out, 2, second), "removed collider is gone from queries");

	bapi_collider_t replacement =
		bapi_collision_world_add(world, make_rect(30, 0, 10, 10), 2, 0xFFFFFFFFu, 1);
	check(replacement != NULL, "add after remove succeeds");
	found = bapi_collision_world_query(world, make_rect(-100, -100, 1000, 1000), 0, out, 8);
	check(found == 3 && contains_handle(out, 3, replacement), "replacement is tracked");

	bapi_collision_world_destroy(world);
}

static void test_capacity_growth(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "growth world_create");
	if (world == NULL) return;

	bapi_collider_t handles[100] = {0};
	for (int i = 0; i < 100; i++) {
		handles[i] = bapi_collision_world_add(world, make_rect((float)(i * 20), 0, 10, 10), 1,
											  0xFFFFFFFFu, 1);
		check(handles[i] != NULL, "bulk add succeeds");
	}

	int found = bapi_collision_world_query(world, make_rect(-100, -100, 2200, 200), 0, NULL, 0);
	check(found == 100, "query counts 100 colliders after array growth");

	bapi_collision_world_remove(world, handles[50]);
	found = bapi_collision_world_query(world, make_rect(-100, -100, 2200, 200), 0, NULL, 0);
	check(found == 99, "query counts 99 colliders after bulk remove");

	bapi_collision_world_destroy(world);
}

static void test_layer_filtering(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "layer world_create");
	if (world == NULL) return;

	bapi_rect_t		spot   = make_rect(0, 0, 10, 10);
	bapi_collider_t layer1 = bapi_collision_world_add(world, spot, 0x1u, 0xFFFFFFFFu, 1);
	bapi_collider_t layer2 = bapi_collision_world_add(world, spot, 0x2u, 0xFFFFFFFFu, 1);
	bapi_collider_t layer4 = bapi_collision_world_add(world, spot, 0x4u, 0xFFFFFFFFu, 1);

	bapi_rect_t		wide   = make_rect(-10, -10, 40, 30);
	bapi_collider_t out[4] = {0};

	check(bapi_collision_world_query(world, wide, 0x1u, out, 4) == 1 && out[0] == layer1,
		  "layer query matches a single bit");
	check(bapi_collision_world_query(world, wide, 0x2u, out, 4) == 1 && out[0] == layer2,
		  "layer query matches the second collider");
	check(bapi_collision_world_query(world, wide, 0x3u, out, 4) == 2,
		  "layer query matches any requested bit");
	check(bapi_collision_world_query(world, wide, 0x8u, out, 4) == 0,
		  "layer query ignores unset bits");
	check(bapi_collision_world_query(world, wide, 0, out, 4) == 3,
		  "layer zero matches every layer");
	check(bapi_collision_world_query(world, make_rect(100, 100, 10, 10), 0, out, 4) == 0,
		  "layer query still requires overlap");

	bapi_collider_set_layer_mask(layer4, 0x8u, 0xFFFFFFFFu);
	check(bapi_collision_world_query(world, wide, 0x4u, out, 4) == 0,
		  "set_layer_mask removes the old layer");
	check(bapi_collision_world_query(world, wide, 0x8u, out, 4) == 1 && out[0] == layer4,
		  "set_layer_mask installs the new layer");

	bapi_collision_world_destroy(world);
}

static void test_touching_edges(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "touch world_create");
	if (world == NULL) return;

	bapi_collider_t solid =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(solid != NULL, "touch collider added");

	bapi_collider_t out[4] = {0};
	check(bapi_collision_world_query(world, make_rect(10, 0, 10, 10), 0, out, 4) == 0,
		  "right edge touching is not overlap");
	check(bapi_collision_world_query(world, make_rect(-10, 0, 10, 10), 0, out, 4) == 0,
		  "left edge touching is not overlap");
	check(bapi_collision_world_query(world, make_rect(0, 10, 10, 10), 0, out, 4) == 0,
		  "bottom edge touching is not overlap");
	check(bapi_collision_world_query(world, make_rect(0, -10, 10, 10), 0, out, 4) == 0,
		  "top edge touching is not overlap");
	check(bapi_collision_world_query(world, make_rect(9.99f, 0, 10, 10), 0, out, 4) == 1,
		  "slight overlap counts");
	check(bapi_collision_world_query(world, make_rect(2, 2, 2, 2), 0, out, 4) == 1,
		  "contained rectangle overlaps");
	check(bapi_collision_world_query(world, make_rect(10.01f, 0, 10, 10), 0, out, 4) == 0,
		  "small gap is not overlap");

	bapi_collision_world_destroy(world);
}

static void test_move_free(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "free move world_create");
	if (world == NULL) return;

	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	int hit		 = -1;
	int contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){5.0f, 3.0f}, &hit);
	check(contacts == 0, "free move reports no contacts");
	check(hit == 0, "free move is not blocked");

	bapi_rect_t bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.x, 5.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 3.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.w, 10.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.h, 10.0f, CHECK_TOLERANCE),
		  "free move updates bounds");

	hit		 = -1;
	contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){-5.0f, -3.0f}, &hit);
	bounds	 = bapi_collider_get_bounds(player);
	check(contacts == 0 && hit == 0, "negative free move is not blocked");
	check(nearly_equal(bounds.x, 0.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 0.0f, CHECK_TOLERANCE),
		  "negative free move updates bounds");

	hit		 = -1;
	contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){0.0f, 0.0f}, &hit);
	check(contacts == 0 && hit == 0, "zero delta is a no-op");
	bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.x, 0.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 0.0f, CHECK_TOLERANCE),
		  "zero delta leaves bounds untouched");

	bapi_collision_world_destroy(world);
}

static void test_move_blocked_x(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "blocked x world_create");
	if (world == NULL) return;

	bapi_collider_t wall =
		bapi_collision_world_add(world, make_rect(20, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(wall != NULL && player != NULL, "blocked x colliders added");

	int hit		 = 0;
	int contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){15.0f, 0.0f}, &hit);
	check(contacts >= 1, "blocked x reports a contact");
	check(hit == 1, "blocked x sets hit");

	bapi_rect_t bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.x, 10.0f, 0.01f), "blocked x stops at the wall");
	check(bounds.x + bounds.w <= 20.0f + CHECK_TOLERANCE, "blocked x does not overlap the wall");
	check(nearly_equal(bounds.y, 0.0f, CHECK_TOLERANCE), "blocked x leaves y alone");

	hit		 = 0;
	contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){5.0f, 0.0f}, &hit);
	check(contacts >= 1 && hit == 1, "pushing into a touching wall stays blocked");
	check(nearly_equal(bapi_collider_get_bounds(player).x, 10.0f, 0.02f),
		  "pushing into a wall does not drift");

	bapi_collision_world_destroy(world);
}

static void test_move_no_tunneling(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "tunneling world_create");
	if (world == NULL) return;

	bapi_collider_t wall =
		bapi_collision_world_add(world, make_rect(20, 0, 2, 10), 1, 0xFFFFFFFFu, 1);
	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(wall != NULL && player != NULL, "tunneling colliders added");

	int hit		 = 0;
	int contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){100.0f, 0.0f}, &hit);
	check(contacts >= 1 && hit == 1, "large delta is blocked by a thin wall");
	check(nearly_equal(bapi_collider_get_bounds(player).x, 10.0f, 0.01f),
		  "large delta cannot tunnel through a thin wall");

	bapi_collision_world_destroy(world);
}

static void test_move_blocked_y(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "blocked y world_create");
	if (world == NULL) return;

	bapi_collider_t floor =
		bapi_collision_world_add(world, make_rect(0, 20, 100, 10), 1, 0xFFFFFFFFu, 1);
	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(floor != NULL && player != NULL, "blocked y colliders added");

	int hit		 = 0;
	int contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){0.0f, 25.0f}, &hit);
	check(contacts >= 1 && hit == 1, "floor blocks downward movement");

	bapi_rect_t bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.y, 10.0f, 0.01f), "floor stops the player at its top edge");
	check(bounds.y + bounds.h <= 20.0f + CHECK_TOLERANCE, "floor contact does not overlap");

	bapi_collision_world_remove(world, floor);
	bapi_collider_t ceiling =
		bapi_collision_world_add(world, make_rect(0, 0, 100, 10), 1, 0xFFFFFFFFu, 1);
	check(ceiling != NULL, "ceiling collider added");

	bapi_collider_set_bounds(player, make_rect(0, 50, 10, 10));
	hit		 = 0;
	contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){0.0f, -45.0f}, &hit);
	check(contacts >= 1 && hit == 1, "ceiling blocks upward movement");
	bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.y, 10.0f, 0.01f), "ceiling stops the player at its bottom edge");
	check(bounds.y >= 10.0f - CHECK_TOLERANCE, "ceiling contact does not overlap");

	bapi_collision_world_destroy(world);
}

static void test_move_slide(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "slide world_create");
	if (world == NULL) return;

	bapi_collider_t wall =
		bapi_collision_world_add(world, make_rect(20, 0, 10, 100), 1, 0xFFFFFFFFu, 1);
	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(wall != NULL && player != NULL, "slide colliders added");

	int hit		 = 0;
	int contacts = bapi_collision_world_move(world, player, (bapi_vec2_t){15.0f, 10.0f}, &hit);
	check(contacts >= 1 && hit == 1, "diagonal move into a wall reports a block");

	bapi_rect_t bounds = bapi_collider_get_bounds(player);
	check(nearly_equal(bounds.x, 10.0f, 0.01f), "slide stops on x at the wall");
	check(bounds.x + bounds.w <= 20.0f + CHECK_TOLERANCE, "slide never enters the wall");
	check(nearly_equal(bounds.y, 10.0f, CHECK_TOLERANCE), "slide still moves freely on y");

	bapi_collision_world_destroy(world);
}

static void run_filter_case(uint32_t wall_layer, uint32_t wall_mask, int wall_solid,
							int expect_blocked, const char *message)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	if (world == NULL) {
		check(0, "filter world_create");
		return;
	}

	/* Player layer 0x1, mask 0x2: only interacts with layer 0x2 colliders
	 * whose mask includes layer 0x1. */
	bapi_collider_t player =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 0x1u, 0x2u, 1);
	bapi_collider_t wall = bapi_collision_world_add(world, make_rect(20, 0, 10, 10), wall_layer,
													wall_mask, wall_solid);
	check(player != NULL && wall != NULL, "filter colliders added");

	int hit			   = 0;
	int contacts	   = bapi_collision_world_move(world, player, (bapi_vec2_t){15.0f, 0.0f}, &hit);
	bapi_rect_t bounds = bapi_collider_get_bounds(player);

	if (expect_blocked) {
		check(contacts >= 1 && hit == 1 && nearly_equal(bounds.x, 10.0f, 0.01f), message);
	} else {
		check(contacts == 0 && hit == 0 && nearly_equal(bounds.x, 15.0f, CHECK_TOLERANCE), message);
	}

	bapi_collision_world_destroy(world);
}

static void test_move_filtering(void)
{
	run_filter_case(0x2u, 0x1u, 1, 1, "matching layer/mask blocks movement");
	run_filter_case(0x4u, 0x1u, 1, 0, "layer mismatch is ignored");
	run_filter_case(0x2u, 0x4u, 1, 0, "mask mismatch is ignored");
	run_filter_case(0x2u, 0x1u, 0, 0, "non-solid colliders do not block");
}

static void test_collider_accessors(void)
{
	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "accessor world_create");
	if (world == NULL) return;

	bapi_collider_t collider =
		bapi_collision_world_add(world, make_rect(1, 2, 3, 4), 1, 0xFFFFFFFFu, 1);
	check(collider != NULL, "accessor collider added");

	bapi_rect_t bounds = bapi_collider_get_bounds(collider);
	check(nearly_equal(bounds.x, 1.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 2.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.w, 3.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.h, 4.0f, CHECK_TOLERANCE),
		  "get_bounds returns the added bounds");

	bapi_collider_set_bounds(collider, make_rect(5, 6, 7, 8));
	bounds = bapi_collider_get_bounds(collider);
	check(nearly_equal(bounds.x, 5.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 6.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.w, 7.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.h, 8.0f, CHECK_TOLERANCE),
		  "set_bounds updates the collider");

	bapi_collider_t out[2] = {0};
	check(bapi_collision_world_query(world, make_rect(0, 0, 4, 4), 0, out, 2) == 0,
		  "set_bounds moves the collider out of old queries");
	check(bapi_collision_world_query(world, make_rect(0, 0, 20, 20), 0, out, 2) == 1,
		  "set_bounds puts the collider at the new place");

	bapi_collision_world_destroy(world);
}

static void test_null_handling(void)
{
	bapi_collision_world_destroy(NULL);
	bapi_collision_world_remove(NULL, NULL);

	check(bapi_collision_world_add(NULL, make_rect(0, 0, 1, 1), 1, 1, 1) == NULL,
		  "add with NULL world returns NULL");

	bapi_collider_set_bounds(NULL, make_rect(0, 0, 1, 1));
	bapi_collider_set_layer_mask(NULL, 1, 1);
	bapi_rect_t bounds = bapi_collider_get_bounds(NULL);
	check(nearly_equal(bounds.x, 0.0f, 0.0f) && nearly_equal(bounds.y, 0.0f, 0.0f) &&
			  nearly_equal(bounds.w, 0.0f, 0.0f) && nearly_equal(bounds.h, 0.0f, 0.0f),
		  "get_bounds(NULL) returns a zero rect");

	bapi_collider_t out[2] = {0};
	check(bapi_collision_world_query(NULL, make_rect(0, 0, 10, 10), 0, out, 2) == 0,
		  "query with NULL world returns zero");
	check(bapi_collision_world_move(NULL, NULL, (bapi_vec2_t){1.0f, 1.0f}, NULL) == 0,
		  "move with NULL world returns zero");

	bapi_collision_world_t world = bapi_collision_world_create();
	check(world != NULL, "null world_create");
	if (world == NULL) return;

	bapi_collider_t collider =
		bapi_collision_world_add(world, make_rect(0, 0, 10, 10), 1, 0xFFFFFFFFu, 1);
	check(collider != NULL, "null collider added");

	int hit		 = -1;
	int contacts = bapi_collision_world_move(world, NULL, (bapi_vec2_t){1.0f, 1.0f}, &hit);
	check(contacts == 0 && hit == 0, "move with NULL collider clears hit and returns zero");

	bapi_collision_world_remove(world, NULL);
	check(bapi_collision_world_query(world, make_rect(-10, -10, 50, 50), 0, NULL, 0) == 1,
		  "remove(NULL collider) is a no-op");

	bapi_collision_world_remove(NULL, collider);
	check(bapi_collision_world_query(world, make_rect(-10, -10, 50, 50), 0, NULL, 0) == 1,
		  "remove(NULL world) is a no-op");

	check(bapi_collision_world_query(world, make_rect(-10, -10, 50, 50), 0, NULL, 5) == 1,
		  "query with NULL out still counts");
	check(bapi_collision_world_query(world, make_rect(-10, -10, 50, 50), 0, out, 0) == 1,
		  "query with max zero still counts");

	hit		 = -1;
	contacts = bapi_collision_world_move(world, collider, (bapi_vec2_t){2.0f, 2.0f}, NULL);
	check(contacts == 0, "move with NULL hit pointer does not block");
	bounds = bapi_collider_get_bounds(collider);
	check(nearly_equal(bounds.x, 2.0f, CHECK_TOLERANCE) &&
			  nearly_equal(bounds.y, 2.0f, CHECK_TOLERANCE),
		  "move with NULL hit pointer still updates bounds");

	bapi_collision_world_remove(world, collider);
	check(bapi_collision_world_query(world, make_rect(-10, -10, 50, 50), 0, NULL, 0) == 0,
		  "removing the last collider leaves an empty world");

	bapi_collision_world_destroy(world);
}

int main(void)
{
	test_add_remove_query();
	test_capacity_growth();
	test_layer_filtering();
	test_touching_edges();
	test_move_free();
	test_move_blocked_x();
	test_move_no_tunneling();
	test_move_blocked_y();
	test_move_slide();
	test_move_filtering();
	test_collider_accessors();
	test_null_handling();

	if (g_failures != 0) {
		fprintf(stderr, "collision_test: %d of %d checks failed\n", g_failures, g_checks);
		return 1;
	}

	printf("collision_test: %d checks passed\n", g_checks);
	return 0;
}
