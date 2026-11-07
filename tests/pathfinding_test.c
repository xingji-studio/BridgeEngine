/* Grid A* pathfinding tests. */

#include "BridgeEngine.h"

#include <stdio.h>
#include <string.h>

/* Not reached by pathfinding; present so src/tilemap.c links. */
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)color;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	(void)texture;
	(void)params;
}

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "FAIL: %s\n", message);
	return condition ? 0 : 1;
}

static void set_solid(bapi_tilemap_t map, int x, int y)
{
	bapi_tilemap_set_tile(map, x, y, 1, 1);
}

/* Verifies that the path leaves the start implicitly, includes the goal, uses
 * only single-tile 4/8-neighbour steps and never enters a solid tile. */
static int path_is_valid(bapi_tilemap_t map, int start_x, int start_y, int goal_x, int goal_y,
						 bapi_path_t path)
{
	int count = bapi_path_count(path);
	if (count <= 0) return 0;

	int previous_x = start_x;
	int previous_y = start_y;

	for (int index = 0; index < count; index++) {
		int x = 0, y = 0;
		if (bapi_path_get(path, index, &x, &y) != 0) return 0;

		int dx = x - previous_x;
		int dy = y - previous_y;
		if (dx < -1 || dx > 1 || dy < -1 || dy > 1) return 0;
		if (dx == 0 && dy == 0) return 0;
		if (bapi_tilemap_is_solid(map, x, y)) return 0;

		previous_x = x;
		previous_y = y;
	}

	return previous_x == goal_x && previous_y == goal_y;
}

static int test_straight_line(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(5, 5, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	bapi_path_t path = bapi_path_find(map, 0, 0, 4, 0, 0);
	result |= expect(path != NULL, "straight line finds a path");
	result |= expect(bapi_path_count(path) == 4, "straight line has four steps");
	result |= expect(path_is_valid(map, 0, 0, 4, 0, path), "straight line path is valid");

	for (int index = 0; index < 4; index++) {
		int x = -1, y = -1;
		result |= expect(bapi_path_get(path, index, &x, &y) == 0, "path_get succeeds");
		result |= expect(x == index + 1 && y == 0, "straight line stays on row 0");
	}

	/* The start tile is excluded, so no point equals (0, 0). */
	{
		int x = 0, y = 0;
		bapi_path_get(path, 0, &x, &y);
		result |= expect(!(x == 0 && y == 0), "path excludes the start tile");
	}

	bapi_path_destroy(path);

	/* Adjacent goal yields a single step. */
	path = bapi_path_find(map, 2, 2, 3, 2, 0);
	result |= expect(path != NULL && bapi_path_count(path) == 1, "adjacent goal is one step");
	result |= expect(path_is_valid(map, 2, 2, 3, 2, path), "adjacent path is valid");
	bapi_path_destroy(path);

	bapi_tilemap_destroy(map);
	return result;
}

static int test_diagonal(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(6, 6, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* With diagonals enabled an open diagonal run is the shortest path. */
	bapi_path_t path = bapi_path_find(map, 0, 0, 3, 3, 1);
	result |= expect(path != NULL, "diagonal path found");
	result |= expect(bapi_path_count(path) == 3, "diagonal path has three steps");
	result |= expect(path_is_valid(map, 0, 0, 3, 3, path), "diagonal path is valid");

	for (int index = 0; index < 3; index++) {
		int x = -1, y = -1;
		bapi_path_get(path, index, &x, &y);
		result |= expect(x == index + 1 && y == index + 1, "diagonal steps advance on both axes");
	}
	bapi_path_destroy(path);

	/* Single diagonal step beats two orthogonal steps. */
	path = bapi_path_find(map, 0, 0, 1, 1, 1);
	result |= expect(path != NULL && bapi_path_count(path) == 1, "one diagonal step for (1,1)");
	bapi_path_destroy(path);

	/* Without diagonals the same query takes the Manhattan route. */
	path = bapi_path_find(map, 0, 0, 2, 2, 0);
	result |= expect(path != NULL, "orthogonal path found");
	result |= expect(bapi_path_count(path) == 4, "orthogonal path has Manhattan length");
	result |= expect(path_is_valid(map, 0, 0, 2, 2, path), "orthogonal path is valid");
	bapi_path_destroy(path);

	/* A blocked diagonal destination forces a detour even with diagonals on. */
	set_solid(map, 1, 1);
	path = bapi_path_find(map, 0, 0, 2, 2, 1);
	result |= expect(path != NULL, "path around blocked diagonal exists");
	result |=
		expect(path_is_valid(map, 0, 0, 2, 2, path), "detour around blocked diagonal is valid");
	result |= expect(bapi_path_count(path) == 3, "blocked diagonal needs three steps");
	bapi_path_destroy(path);
	bapi_tilemap_set_tile(map, 1, 1, -1, 0);

	bapi_tilemap_destroy(map);
	return result;
}

static int test_obstacle_detour(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(5, 5, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* Wall at column 2, rows 0..3: the only way through is row 4. */
	for (int y = 0; y <= 3; y++) set_solid(map, 2, y);

	bapi_path_t path = bapi_path_find(map, 0, 0, 4, 0, 0);
	result |= expect(path != NULL, "detour path found");
	result |= expect(bapi_path_count(path) == 12, "detour path has the optimal length");
	result |= expect(path_is_valid(map, 0, 0, 4, 0, path), "detour path is valid");

	/* The path must actually step through the open row-4 gap in column 2. */
	int passed_gap = 0;
	for (int index = 0; index < bapi_path_count(path); index++) {
		int x = -1, y = -1;
		bapi_path_get(path, index, &x, &y);
		if (x == 2 && y == 4) passed_gap = 1;
	}
	result |= expect(passed_gap, "detour path crosses the wall through the gap");
	bapi_path_destroy(path);

	/* Determinism: the same query yields the same path. */
	bapi_path_t first  = bapi_path_find(map, 0, 0, 4, 0, 0);
	bapi_path_t second = bapi_path_find(map, 0, 0, 4, 0, 0);
	result |= expect(first != NULL && second != NULL, "repeated query succeeds");
	result |= expect(bapi_path_count(first) == bapi_path_count(second),
					 "repeated query has the same length");
	if (first != NULL && second != NULL) {
		int identical = 1;
		for (int index = 0; index < bapi_path_count(first); index++) {
			int ax = -1, ay = -1, bx = -1, by = -1;
			bapi_path_get(first, index, &ax, &ay);
			bapi_path_get(second, index, &bx, &by);
			if (ax != bx || ay != by) identical = 0;
		}
		result |= expect(identical, "repeated query is deterministic");
	}
	bapi_path_destroy(first);
	bapi_path_destroy(second);

	bapi_tilemap_destroy(map);
	return result;
}

static int test_no_path(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(5, 5, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* Full-height wall splits the map. */
	for (int y = 0; y < 5; y++) set_solid(map, 2, y);

	result |= expect(bapi_path_find(map, 0, 0, 4, 0, 0) == NULL,
					 "no 4-neighbour path through a full wall");
	result |=
		expect(bapi_path_find(map, 0, 0, 4, 0, 1) == NULL, "no diagonal path through a full wall");

	/* Enclosed goal. */
	bapi_tilemap_t room = bapi_tilemap_create(5, 5, 16, 16);
	for (int y = 1; y <= 3; y++)
		for (int x = 1; x <= 3; x++) set_solid(room, x, y);
	bapi_tilemap_set_tile(room, 2, 2, 1, 0); /* walkable goal inside the ring */
	result |= expect(bapi_path_find(room, 0, 0, 2, 2, 1) == NULL, "enclosed goal is unreachable");

	bapi_tilemap_destroy(room);
	bapi_tilemap_destroy(map);
	return result;
}

static int test_bounds_and_solid_endpoints(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(4, 4, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	result |= expect(bapi_path_find(NULL, 0, 0, 1, 1, 0) == NULL, "NULL map is rejected");
	result |= expect(bapi_path_find(map, -1, 0, 1, 1, 0) == NULL, "negative start x is rejected");
	result |= expect(bapi_path_find(map, 0, -1, 1, 1, 0) == NULL, "negative start y is rejected");
	result |= expect(bapi_path_find(map, 4, 0, 1, 1, 0) == NULL, "start x >= width is rejected");
	result |= expect(bapi_path_find(map, 0, 4, 1, 1, 0) == NULL, "start y >= height is rejected");
	result |= expect(bapi_path_find(map, 0, 0, -1, 1, 0) == NULL, "negative goal x is rejected");
	result |= expect(bapi_path_find(map, 0, 0, 0, 4, 0) == NULL, "goal y >= height is rejected");
	result |= expect(bapi_path_find(map, 0, 0, 4, 0, 0) == NULL, "goal x >= width is rejected");

	/* Solid endpoints are rejected, documented behavior. */
	set_solid(map, 0, 0);
	result |= expect(bapi_path_find(map, 0, 0, 3, 3, 0) == NULL, "solid start is rejected");
	bapi_tilemap_set_tile(map, 0, 0, -1, 0);

	set_solid(map, 3, 3);
	result |= expect(bapi_path_find(map, 0, 0, 3, 3, 0) == NULL, "solid goal is rejected");
	bapi_tilemap_set_tile(map, 3, 3, -1, 0);

	bapi_tilemap_destroy(map);
	return result;
}

static int test_empty_path_and_accessors(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(4, 4, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* start == goal: a valid empty path, not NULL. */
	bapi_path_t path = bapi_path_find(map, 2, 2, 2, 2, 0);
	result |= expect(path != NULL, "start == goal returns a path handle");
	result |= expect(bapi_path_count(path) == 0, "start == goal path is empty");

	int x = 123, y = 456;
	result |=
		expect(bapi_path_get(path, 0, &x, &y) == -1, "path_get rejects index 0 on empty path");
	result |= expect(x == 123 && y == 456, "failed path_get does not modify outputs");
	bapi_path_destroy(path);

	/* Large map, far goal, then index checks. */
	bapi_tilemap_t big = bapi_tilemap_create(16, 16, 16, 16);
	path			   = bapi_path_find(big, 0, 0, 15, 15, 0);
	result |= expect(path != NULL, "long path found");
	int count = bapi_path_count(path);
	result |= expect(count == 30, "Manhattan path across 16x16 is 30 steps");
	result |= expect(bapi_path_get(path, count - 1, &x, &y) == 0, "last index is accessible");
	result |= expect(x == 15 && y == 15, "last path point is the goal");
	result |= expect(bapi_path_get(path, count, &x, &y) == -1, "index == count is rejected");
	result |= expect(bapi_path_get(path, -1, &x, &y) == -1, "negative index is rejected");
	result |= expect(bapi_path_get(path, 0, NULL, NULL) == 0, "NULL out pointers are allowed");
	bapi_path_destroy(path);
	bapi_tilemap_destroy(big);

	/* NULL handle accessors. */
	result |= expect(bapi_path_count(NULL) == 0, "count(NULL) is 0");
	result |= expect(bapi_path_get(NULL, 0, &x, &y) == -1, "get(NULL) is -1");
	bapi_path_destroy(NULL); /* must not crash */

	bapi_tilemap_destroy(map);
	return result;
}

static int test_diagonal_cost_preference(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(10, 10, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* Open diagonal from (0,0) to (5,5) must use five diagonal steps. */
	bapi_path_t path = bapi_path_find(map, 0, 0, 5, 5, 1);
	result |= expect(path != NULL, "open diagonal path found");
	result |= expect(bapi_path_count(path) == 5, "diagonal path is five steps");
	result |= expect(path_is_valid(map, 0, 0, 5, 5, path), "open diagonal path is valid");
	bapi_path_destroy(path);

	/* Mostly-diagonal query (dx 5, dy 2) is 2 diagonals + 3 straights = 5. */
	path = bapi_path_find(map, 0, 0, 5, 2, 1);
	result |= expect(path != NULL, "off-axis diagonal path found");
	result |= expect(bapi_path_count(path) == 5, "octile distance is used with diagonals");
	result |= expect(path_is_valid(map, 0, 0, 5, 2, path), "off-axis diagonal path is valid");
	bapi_path_destroy(path);

	bapi_tilemap_destroy(map);
	return result;
}

int main(void)
{
	int result = 0;

	result |= test_straight_line();
	result |= test_diagonal();
	result |= test_obstacle_detour();
	result |= test_no_path();
	result |= test_bounds_and_solid_endpoints();
	result |= test_empty_path_and_accessors();
	result |= test_diagonal_cost_preference();

	if (result == 0) {
		printf("pathfinding tests passed\n");
		return 0;
	}

	fprintf(stderr, "pathfinding tests failed\n");
	return 1;
}
