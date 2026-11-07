/* Tilemap runtime tests. */

#include "BridgeEngine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* bapi_texture_t is an opaque pointer; the mocked bapi_texture_draw never
 * dereferences it, so a dummy definition is enough for the test. */
struct bapi_texture_internal {
	int unused;
};

#define MAX_RECORDS 64

typedef struct {
	bapi_rect_t	 dst;
	bapi_rect_t	 src;
	bapi_color_t color;
	int			 has_color;
} draw_record_t;

static draw_record_t g_fill_records[MAX_RECORDS];
static int			 g_fill_count;

static draw_record_t g_texture_records[MAX_RECORDS];
static int			 g_texture_count;

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "FAIL: %s\n", message);
	return condition ? 0 : 1;
}

static void reset_draw_records(void)
{
	memset(g_fill_records, 0, sizeof(g_fill_records));
	memset(g_texture_records, 0, sizeof(g_texture_records));
	g_fill_count	= 0;
	g_texture_count = 0;
}

void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	if (g_fill_count < MAX_RECORDS) {
		g_fill_records[g_fill_count].dst	   = (bapi_rect_t){x, y, w, h};
		g_fill_records[g_fill_count].color	   = color;
		g_fill_records[g_fill_count].has_color = 1;
	}
	g_fill_count++;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	if (texture == NULL || params == NULL) return;
	if (g_texture_count < MAX_RECORDS) {
		g_texture_records[g_texture_count].dst = params->dst;
		g_texture_records[g_texture_count].src = params->src;
	}
	g_texture_count++;
}

static int near(float a, float b)
{
	return fabsf(a - b) <= 0.01f;
}

static int test_create_and_bounds(void)
{
	int result = 0;

	result |= expect(bapi_tilemap_create(0, 3, 16, 16) == NULL, "zero width is rejected");
	result |= expect(bapi_tilemap_create(3, 0, 16, 16) == NULL, "zero height is rejected");
	result |= expect(bapi_tilemap_create(-2, 3, 16, 16) == NULL, "negative width is rejected");
	result |= expect(bapi_tilemap_create(3, 3, 0, 16) == NULL, "zero tile width is rejected");
	result |= expect(bapi_tilemap_create(3, 3, 16, -4) == NULL, "negative tile height is rejected");

	bapi_tilemap_t map = bapi_tilemap_create(4, 3, 16, 16);
	result |= expect(map != NULL, "valid tilemap is created");
	if (map == NULL) return 1;

	result |= expect(bapi_tilemap_get_tile(map, 0, 0) == -1, "new tiles are empty");
	result |= expect(bapi_tilemap_get_tile(map, 3, 2) == -1, "last new tile is empty");
	result |= expect(bapi_tilemap_is_solid(map, 0, 0) == 0, "new tiles are not solid");

	result |= expect(bapi_tilemap_get_tile(map, -1, 0) == -1, "get_tile rejects x < 0");
	result |= expect(bapi_tilemap_get_tile(map, 0, -1) == -1, "get_tile rejects y < 0");
	result |= expect(bapi_tilemap_get_tile(map, 4, 0) == -1, "get_tile rejects x >= width");
	result |= expect(bapi_tilemap_get_tile(map, 0, 3) == -1, "get_tile rejects y >= height");
	result |= expect(bapi_tilemap_is_solid(map, -1, -1) == 0, "is_solid rejects negative");
	result |= expect(bapi_tilemap_is_solid(map, 4, 3) == 0, "is_solid rejects out of grid");

	/* Out-of-bounds writes are ignored and must not corrupt the grid. */
	bapi_tilemap_set_tile(map, -1, 0, 9, 1);
	bapi_tilemap_set_tile(map, 0, 99, 9, 1);
	bapi_tilemap_set_tile(map, 4, 0, 9, 1);
	result |= expect(bapi_tilemap_get_tile(map, 0, 0) == -1, "out-of-bounds set_tile is ignored");

	bapi_tilemap_destroy(map);
	bapi_tilemap_destroy(NULL); /* must not crash */

	return result;
}

static int test_set_and_get_tile(void)
{
	int result = 0;

	bapi_tilemap_t map = bapi_tilemap_create(4, 3, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	bapi_tilemap_set_tile(map, 2, 1, 7, 0);
	result |= expect(bapi_tilemap_get_tile(map, 2, 1) == 7, "set_tile stores the tile id");
	result |= expect(bapi_tilemap_is_solid(map, 2, 1) == 0, "non-solid flag stored");

	bapi_tilemap_set_tile(map, 1, 0, 3, 1);
	result |= expect(bapi_tilemap_get_tile(map, 1, 0) == 3, "second tile stored");
	result |= expect(bapi_tilemap_is_solid(map, 1, 0) == 1, "solid flag stored");

	bapi_tilemap_set_tile(map, 2, 1, 8, 1);
	result |= expect(bapi_tilemap_get_tile(map, 2, 1) == 8, "set_tile overwrites the tile id");
	result |= expect(bapi_tilemap_is_solid(map, 2, 1) == 1, "set_tile overwrites the solid flag");

	bapi_tilemap_set_tile(map, 2, 1, -1, 0);
	result |= expect(bapi_tilemap_get_tile(map, 2, 1) == -1, "tile can be cleared to -1");
	result |= expect(bapi_tilemap_is_solid(map, 2, 1) == 0, "cleared tile is not solid");

	bapi_tilemap_destroy(map);
	return result;
}

static int test_collide_rect(void)
{
	int result = 0;

	bapi_tilemap_t map = bapi_tilemap_create(4, 3, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	bapi_tilemap_set_tile(map, 1, 1, 1, 1); /* tile occupies x/y 16..32 */

	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){0, 0, 8, 8}) == 0,
					 "empty corner does not collide");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){16, 16, 16, 16}) == 1,
					 "exact solid tile collides");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){12, 12, 8, 8}) == 1,
					 "partial overlap collides");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){24, 24, 4, 4}) == 1,
					 "overlap in the middle of a tile collides");

	/* Touching borders are not overlaps: half-open AABB. */
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){8, 8, 8, 8}) == 0,
					 "rect ending exactly at the tile edge does not collide");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){32, 16, 8, 8}) == 0,
					 "rect starting exactly at the tile right edge does not collide");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){16, 32, 8, 8}) == 0,
					 "rect starting exactly at the tile bottom edge does not collide");

	/* Zero/negative sized rects never collide. */
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){16, 16, 0, 0}) == 0,
					 "zero sized rect does not collide");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){16, 16, -4, 4}) == 0,
					 "negative sized rect does not collide");

	/* Rect fully outside the grid is not solid. */
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){-64, -64, 8, 8}) == 0,
					 "rect outside the grid does not collide");
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){0, 0, 128, 128}) == 1,
					 "huge rect covering the map collides");

	/* A rect spanning multiple tiles hits a solid one in the middle. */
	bapi_tilemap_set_tile(map, 3, 2, 1, 1);
	result |= expect(bapi_tilemap_collide_rect(map, (bapi_rect_t){0, 0, 64, 48}) == 1,
					 "multi-tile rect detects a solid tile");

	bapi_tilemap_destroy(map);
	return result;
}

static int test_move_rect_axis_resolution(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(8, 8, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* Vertical wall at column 2 (x 32..48). */
	for (int y = 0; y < 8; y++) bapi_tilemap_set_tile(map, 2, y, 1, 1);
	/* Horizontal floor at row 5 (y 80..96). */
	for (int x = 0; x < 8; x++) bapi_tilemap_set_tile(map, x, 5, 1, 1);
	/* Ceiling at row 0 is not solid; left wall at column 0 is not solid. */

	/* Free movement: full delta applied on both axes. */
	{
		bapi_rect_t rect	= {0.0f, 0.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){10.0f, 10.0f});
		result |= expect(blocked == 0, "free move is not blocked");
		result |=
			expect(near(rect.x, 10.0f) && near(rect.y, 10.0f), "free move applies full delta");
	}

	/* X blocked by the wall: rect stops at the tile edge minus epsilon. */
	{
		bapi_rect_t rect	= {0.0f, 0.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){100.0f, 0.0f});
		result |= expect(blocked == 1, "wall blocks horizontal move");
		result |= expect(near(rect.x, 24.0f), "horizontal move stops at the wall (x == 24)");
		result |= expect(near(rect.y, 0.0f), "blocked X move leaves y untouched");
	}

	/* A single large step must not tunnel through a one-tile wall. */
	{
		bapi_rect_t rect	= {0.0f, 32.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){1000.0f, 0.0f});
		result |= expect(blocked == 1, "large step does not tunnel through the wall");
		result |= expect(rect.x < 32.0f, "large step leaves the rect on the near side of the wall");
		result |= expect(rect.x > 23.0f, "large step still advances to the wall");
	}

	/* Y blocked by the floor. */
	{
		bapi_rect_t rect	= {0.0f, 48.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){0.0f, 100.0f});
		result |= expect(blocked == 1, "floor blocks vertical move");
		result |= expect(near(rect.y, 72.0f), "vertical move stops at the floor (y == 72)");
	}

	/* X blocked, Y free: the axes resolve independently and the call reports blocked. */
	{
		bapi_rect_t rect	= {0.0f, 0.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){100.0f, 10.0f});
		result |= expect(blocked == 1, "diagonal move with a blocked axis reports blocked");
		result |= expect(near(rect.x, 24.0f), "blocked X axis stops at the wall");
		result |= expect(near(rect.y, 10.0f), "free Y axis applies the full delta");
	}

	/* Negative X towards a wall on the left of the start position. */
	{
		bapi_tilemap_set_tile(map, 0, 0, 1, 1); /* wall at x 0..16, y 0..16 */
		bapi_rect_t rect	= {16.0f, 0.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){-100.0f, 0.0f});
		result |= expect(blocked == 1, "negative move is blocked by the left wall");
		result |= expect(near(rect.x, 16.0f), "negative move stops at the wall (x == 16)");
		bapi_tilemap_set_tile(map, 0, 0, -1, 0);
	}

	/* Zero delta never blocks and never moves. */
	{
		bapi_rect_t rect	= {20.0f, 20.0f, 8.0f, 8.0f};
		int			blocked = bapi_tilemap_move_rect(map, &rect, (bapi_vec2_t){0.0f, 0.0f});
		result |= expect(blocked == 0, "zero delta is not blocked");
		result |= expect(near(rect.x, 20.0f) && near(rect.y, 20.0f), "zero delta does not move");
	}

	/* Null / degenerate inputs are no-ops. */
	{
		bapi_rect_t rect = {0.0f, 0.0f, 8.0f, 8.0f};
		result |= expect(bapi_tilemap_move_rect(NULL, &rect, (bapi_vec2_t){1.0f, 1.0f}) == 0,
						 "NULL tilemap returns 0");
		result |= expect(bapi_tilemap_move_rect(map, NULL, (bapi_vec2_t){1.0f, 1.0f}) == 0,
						 "NULL rect returns 0");
		result |= expect(bapi_tilemap_move_rect(map, &(bapi_rect_t){0.0f, 0.0f, 0.0f, 0.0f},
												(bapi_vec2_t){1.0f, 1.0f}) == 0,
						 "zero sized rect returns 0");
	}

	bapi_tilemap_destroy(map);
	return result;
}

static int test_draw_fill_path(void)
{
	int result = 0;

	bapi_tilemap_t map = bapi_tilemap_create(4, 3, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	/* Draw order is row-major: (0,0), (1,0), (3,2). */
	bapi_tilemap_set_tile(map, 0, 0, 5, 0);
	bapi_tilemap_set_tile(map, 1, 0, 2, 0);
	bapi_tilemap_set_tile(map, 3, 2, 7, 0);
	/* Tile (2,1) stays empty and must not be drawn. */

	reset_draw_records();
	bapi_tilemap_draw(map);

	result |= expect(g_fill_count == 3, "fill path draws only non-empty tiles");
	result |= expect(g_texture_count == 0, "fill path does not call texture draw");

	result |= expect(g_fill_records[0].dst.x == 0.0f && g_fill_records[0].dst.y == 0.0f,
					 "first tile draws at the grid origin");
	result |= expect(g_fill_records[0].dst.w == 16.0f && g_fill_records[0].dst.h == 16.0f,
					 "tile draw uses tile dimensions");
	result |= expect(g_fill_records[1].dst.x == 16.0f && g_fill_records[1].dst.y == 0.0f,
					 "second tile draws one tile to the right");
	result |= expect(g_fill_records[2].dst.x == 48.0f && g_fill_records[2].dst.y == 32.0f,
					 "last tile draws at row-major coordinates");

	/* The fallback color is deterministic for a given tile id. */
	result |= expect(g_fill_records[0].color.a == 255, "fallback color is opaque");
	bapi_color_t first_color = g_fill_records[0].color;
	reset_draw_records();
	bapi_tilemap_draw(map);
	result |= expect(g_fill_count == 3, "second draw has the same tile count");
	result |= expect(g_fill_records[0].color.r == first_color.r &&
						 g_fill_records[0].color.g == first_color.g &&
						 g_fill_records[0].color.b == first_color.b,
					 "fallback color is deterministic");

	/* Draw must not be affected by an empty map. */
	bapi_tilemap_t empty = bapi_tilemap_create(2, 2, 8, 8);
	reset_draw_records();
	bapi_tilemap_draw(empty);
	result |= expect(g_fill_count == 0 && g_texture_count == 0, "empty map draws nothing");
	bapi_tilemap_destroy(empty);

	/* NULL tilemap is a no-op. */
	reset_draw_records();
	bapi_tilemap_draw(NULL);
	result |= expect(g_fill_count == 0 && g_texture_count == 0, "NULL map draws nothing");

	bapi_tilemap_destroy(map);
	return result;
}

static int test_draw_texture_path(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(4, 3, 16, 16);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	static struct bapi_texture_internal fake_texture;
	bapi_texture_t						texture = &fake_texture;

	bapi_tilemap_set_tile(map, 0, 0, 5, 0); /* src tile (1,1) in a 4 column sheet */
	bapi_tilemap_set_tile(map, 1, 0, 2, 0); /* src tile (2,0) */
	bapi_tilemap_set_tile(map, 3, 2, 7, 0); /* src tile (3,1) */

	result |= expect(bapi_tilemap_set_tileset(map, texture, 4) == 0, "set_tileset accepts columns");
	result |=
		expect(bapi_tilemap_set_tileset(map, texture, 0) == -1, "set_tileset rejects zero columns");
	result |= expect(bapi_tilemap_set_tileset(map, texture, -1) == -1,
					 "set_tileset rejects negative columns");
	result |= expect(bapi_tilemap_set_tileset(NULL, texture, 4) == -1,
					 "set_tileset rejects a NULL tilemap");

	/* Invalid calls did not change the configured tileset. */
	reset_draw_records();
	bapi_tilemap_draw(map);

	result |= expect(g_texture_count == 3, "texture path draws only non-empty tiles");
	result |= expect(g_fill_count == 0, "texture path does not call fill");

	result |= expect(g_texture_records[0].dst.x == 0.0f && g_texture_records[0].dst.y == 0.0f,
					 "textured tile draws at the grid origin");
	result |= expect(g_texture_records[0].dst.w == 16.0f && g_texture_records[0].dst.h == 16.0f,
					 "textured tile uses tile dimensions");

	/* tile id 5 with 4 columns -> column 1, row 1. */
	result |= expect(g_texture_records[0].src.x == 16.0f && g_texture_records[0].src.y == 16.0f,
					 "tile id 5 maps to source cell (1,1)");
	result |= expect(g_texture_records[0].src.w == 16.0f && g_texture_records[0].src.h == 16.0f,
					 "source cell uses tile dimensions");

	/* tile id 2 -> column 2, row 0. */
	result |= expect(g_texture_records[1].src.x == 32.0f && g_texture_records[1].src.y == 0.0f,
					 "tile id 2 maps to source cell (2,0)");

	/* tile id 7 -> column 3, row 1; destination at (3*16, 2*16). */
	result |= expect(g_texture_records[2].src.x == 48.0f && g_texture_records[2].src.y == 16.0f,
					 "tile id 7 maps to source cell (3,1)");
	result |= expect(g_texture_records[2].dst.x == 48.0f && g_texture_records[2].dst.y == 32.0f,
					 "textured tile destination follows row-major order");

	/* Clearing the tileset (NULL texture) switches back to the fill path. */
	result |=
		expect(bapi_tilemap_set_tileset(map, NULL, 4) == 0, "set_tileset accepts NULL texture");
	reset_draw_records();
	bapi_tilemap_draw(map);
	result |= expect(g_texture_count == 0, "cleared tileset stops texture draws");
	result |= expect(g_fill_count == 3, "cleared tileset falls back to fill draws");

	bapi_tilemap_destroy(map);
	return result;
}

int main(void)
{
	int result = 0;

	result |= test_create_and_bounds();
	result |= test_set_and_get_tile();
	result |= test_collide_rect();
	result |= test_move_rect_axis_resolution();
	result |= test_draw_fill_path();
	result |= test_draw_texture_path();

	if (result == 0) {
		printf("tilemap tests passed\n");
		return 0;
	}

	fprintf(stderr, "tilemap tests failed\n");
	return 1;
}
