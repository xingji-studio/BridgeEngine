/* Row-major tile grid. tile_id -1 is empty; tiles outside the grid are empty.
 * move_rect resolves X then Y in sub-tile chunks so large deltas cannot tunnel.
 * The tileset texture is borrowed.
 */

#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/tilemap_internal.h"

#include <stdint.h>
#include <stdlib.h>

#define BAPI_TILEMAP_EPSILON		   0.001f
#define BAPI_TILEMAP_STEP_FACTOR	   0.5f
#define BAPI_TILEMAP_REFINE_ITERATIONS 16



static float tilemap_absf(float value)
{
	return value < 0.0f ? -value : value;
}

/* floor(value / size) without <math.h>, which is incomplete on XJ380. */
static int tilemap_floor_index(float value, int size)
{
	int index = (int)(value / (float)size);
	if ((float)index * (float)size > value) index--;
	return index;
}

static float tilemap_step_size(bapi_tilemap_t tilemap)
{
	int dimension =
		tilemap->tile_width < tilemap->tile_height ? tilemap->tile_width : tilemap->tile_height;
	if (dimension <= 0) return 1.0f;
	return (float)dimension * BAPI_TILEMAP_STEP_FACTOR;
}

static int tilemap_rect_collides(bapi_tilemap_t tilemap, float x, float y, float w, float h)
{
	bapi_rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	return bapi_tilemap_collide_rect(tilemap, rect);
}

/* Move `rect` along one axis. Returns 1 when the motion was stopped early. */
static int tilemap_move_axis(bapi_tilemap_t tilemap, bapi_rect_t *rect, float amount,
							 int horizontal)
{
	if (amount == 0.0f) return 0;

	float step		= tilemap_step_size(tilemap);
	float remaining = amount;

	while (tilemap_absf(remaining) > BAPI_TILEMAP_EPSILON) {
		float chunk = remaining;
		if (chunk > step)
			chunk = step;
		else if (chunk < -step)
			chunk = -step;

		float probe_x = rect->x + (horizontal ? chunk : 0.0f);
		float probe_y = rect->y + (horizontal ? 0.0f : chunk);

		if (!tilemap_rect_collides(tilemap, probe_x, probe_y, rect->w, rect->h)) {
			rect->x = probe_x;
			rect->y = probe_y;
			remaining -= chunk;
			continue;
		}

		/* Refine the furthest free travel inside the blocked chunk. */
		float lo = 0.0f;
		float hi = chunk;
		for (int i = 0; i < BAPI_TILEMAP_REFINE_ITERATIONS; i++) {
			float mid	 = (lo + hi) * 0.5f;
			float test_x = rect->x + (horizontal ? mid : 0.0f);
			float test_y = rect->y + (horizontal ? 0.0f : mid);
			if (tilemap_rect_collides(tilemap, test_x, test_y, rect->w, rect->h)) {
				hi = mid;
			} else {
				lo = mid;
			}
		}

		float travel = lo;
		if (tilemap_absf(travel) > BAPI_TILEMAP_EPSILON) {
			travel -= travel > 0.0f ? BAPI_TILEMAP_EPSILON : -BAPI_TILEMAP_EPSILON;
		} else {
			travel = 0.0f;
		}

		if (horizontal)
			rect->x += travel;
		else
			rect->y += travel;
		return 1;
	}

	return 0;
}

/* Deterministic, opaque color used when no tileset is assigned. */
static bapi_color_t tilemap_tile_color(int tile_id)
{
	uint32_t	 hash = (uint32_t)tile_id * 2654435761u + 0x9E3779B9u;
	bapi_color_t color;
	color.r = (uint8_t)(48u + ((hash >> 0) & 0x7Fu));
	color.g = (uint8_t)(48u + ((hash >> 8) & 0x7Fu));
	color.b = (uint8_t)(48u + ((hash >> 16) & 0x7Fu));
	color.a = 255;
	return color;
}

bapi_tilemap_t bapi_tilemap_create(int width, int height, int tile_width, int tile_height)
{
	if (width <= 0 || height <= 0 || tile_width <= 0 || tile_height <= 0) {
		bapi_set_last_error("bapi_tilemap_create: dimensions must be positive");
		return NULL;
	}

	if ((size_t)width > (size_t)-1 / (size_t)height) {
		bapi_set_last_error("bapi_tilemap_create: grid is too large");
		return NULL;
	}

	bapi_tilemap_t tilemap = calloc(1, sizeof(*tilemap));
	if (tilemap == NULL) return NULL;

	size_t count = (size_t)width * (size_t)height;
	if (count > (size_t)-1 / sizeof(int)) {
		free(tilemap);
		bapi_set_last_error("bapi_tilemap_create: grid is too large");
		return NULL;
	}

	tilemap->width		 = width;
	tilemap->height		 = height;
	tilemap->tile_width	 = tile_width;
	tilemap->tile_height = tile_height;
	tilemap->tileset	 = NULL;
	tilemap->columns	 = 0;

	tilemap->tiles = calloc(count, sizeof(int));
	tilemap->solid = calloc(count, sizeof(uint8_t));
	if (tilemap->tiles == NULL || tilemap->solid == NULL) {
		free(tilemap->tiles);
		free(tilemap->solid);
		free(tilemap);
		return NULL;
	}

	for (size_t i = 0; i < count; i++) tilemap->tiles[i] = -1;

	return tilemap;
}

void bapi_tilemap_destroy(bapi_tilemap_t tilemap)
{
	if (tilemap == NULL) return;
	free(tilemap->tiles);
	free(tilemap->solid);
	free(tilemap);
}

int bapi_tilemap_set_tileset(bapi_tilemap_t tilemap, bapi_texture_t texture, int columns)
{
	if (tilemap == NULL) {
		bapi_set_last_error("bapi_tilemap_set_tileset: tilemap is NULL");
		return -1;
	}
	if (columns <= 0) {
		bapi_set_last_error("bapi_tilemap_set_tileset: columns must be positive");
		return -1;
	}

	tilemap->tileset = texture;
	tilemap->columns = columns;
	return 0;
}

void bapi_tilemap_set_tile(bapi_tilemap_t tilemap, int x, int y, int tile_id, int solid)
{
	if (tilemap == NULL) return;
	if (x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) return;

	int index			  = y * tilemap->width + x;
	tilemap->tiles[index] = tile_id;
	tilemap->solid[index] = solid ? 1 : 0;
}

int bapi_tilemap_get_tile(bapi_tilemap_t tilemap, int x, int y)
{
	if (tilemap == NULL) return -1;
	if (x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) return -1;
	return tilemap->tiles[y * tilemap->width + x];
}

int bapi_tilemap_is_solid(bapi_tilemap_t tilemap, int x, int y)
{
	if (tilemap == NULL) return 0;
	if (x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) return 0;
	return tilemap->solid[y * tilemap->width + x] ? 1 : 0;
}

int bapi_tilemap_collide_rect(bapi_tilemap_t tilemap, bapi_rect_t rect)
{
	if (tilemap == NULL) return 0;
	if (rect.w <= 0.0f || rect.h <= 0.0f) return 0;

	/* Half-open AABB: the right/bottom edges are exclusive. The epsilon keeps
	 * a rect that merely touches a tile border out of the collision. */
	float right_edge =
		rect.w > BAPI_TILEMAP_EPSILON ? rect.x + rect.w - BAPI_TILEMAP_EPSILON : rect.x;
	float bottom_edge =
		rect.h > BAPI_TILEMAP_EPSILON ? rect.y + rect.h - BAPI_TILEMAP_EPSILON : rect.y;

	int left   = tilemap_floor_index(rect.x, tilemap->tile_width);
	int right  = tilemap_floor_index(right_edge, tilemap->tile_width);
	int top	   = tilemap_floor_index(rect.y, tilemap->tile_height);
	int bottom = tilemap_floor_index(bottom_edge, tilemap->tile_height);

	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right >= tilemap->width) right = tilemap->width - 1;
	if (bottom >= tilemap->height) bottom = tilemap->height - 1;

	for (int ty = top; ty <= bottom; ty++) {
		for (int tx = left; tx <= right; tx++) {
			if (tilemap->solid[ty * tilemap->width + tx]) return 1;
		}
	}

	return 0;
}

int bapi_tilemap_move_rect(bapi_tilemap_t tilemap, bapi_rect_t *rect, bapi_vec2_t delta)
{
	if (tilemap == NULL || rect == NULL) return 0;
	if (rect->w <= 0.0f || rect->h <= 0.0f) return 0;

	int blocked = tilemap_move_axis(tilemap, rect, delta.x, 1);
	blocked |= tilemap_move_axis(tilemap, rect, delta.y, 0);
	return blocked ? 1 : 0;
}

void bapi_tilemap_draw(bapi_tilemap_t tilemap)
{
	if (tilemap == NULL) return;

	int	  has_tileset = tilemap->tileset != NULL && tilemap->columns > 0;
	float tile_w	  = (float)tilemap->tile_width;
	float tile_h	  = (float)tilemap->tile_height;

	for (int y = 0; y < tilemap->height; y++) {
		for (int x = 0; x < tilemap->width; x++) {
			int tile_id = tilemap->tiles[y * tilemap->width + x];
			if (tile_id < 0) continue;

			bapi_rect_t dst;
			dst.x = (float)x * tile_w;
			dst.y = (float)y * tile_h;
			dst.w = tile_w;
			dst.h = tile_h;

			if (has_tileset) {
				bapi_texture_draw_t params;
				params.src.x	= (float)(tile_id % tilemap->columns) * tile_w;
				params.src.y	= (float)(tile_id / tilemap->columns) * tile_h;
				params.src.w	= tile_w;
				params.src.h	= tile_h;
				params.dst		= dst;
				params.pivot_x	= 0.0f;
				params.pivot_y	= 0.0f;
				params.rotation = 0.0f;
				params.flip		= 0;
				params.tint		= (bapi_color_t){255, 255, 255, 255};
				bapi_texture_draw(tilemap->tileset, &params);
			} else {
				bapi_fill_rect(dst.x, dst.y, dst.w, dst.h, tilemap_tile_color(tile_id));
			}
		}
	}
}
