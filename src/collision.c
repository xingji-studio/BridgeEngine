#include "BridgeEngine.h"

#include <stdlib.h>

/* Overlap tolerance: touching edges are not a collision. */
#define COLLISION_EPSILON 1e-4f

struct bapi_collider_internal {
	bapi_rect_t bounds;
	uint32_t	layer;
	uint32_t	mask;
	int			solid;
};

struct bapi_collision_world_internal {
	bapi_collider_t *colliders;
	int				 count;
	int				 capacity;
};

/* Strict AABB overlap with a small epsilon. Degenerate rectangles (zero or
 * negative width/height) never overlap anything. */
static int rects_overlap(bapi_rect_t a, bapi_rect_t b)
{
	if (a.w <= 0.0f || a.h <= 0.0f || b.w <= 0.0f || b.h <= 0.0f) return 0;
	return a.x < b.x + b.w - COLLISION_EPSILON && b.x < a.x + a.w - COLLISION_EPSILON &&
		   a.y < b.y + b.h - COLLISION_EPSILON && b.y < a.y + a.h - COLLISION_EPSILON;
}

/* Strict 1D interval overlap, used for the axis perpendicular to a sweep. */
static int spans_overlap(float a0, float a1, float b0, float b1)
{
	return a1 > b0 + COLLISION_EPSILON && b1 > a0 + COLLISION_EPSILON;
}

/* A and B interact only when each one's mask includes the other's layer. */
static int filter_pass(bapi_collider_t a, bapi_collider_t b)
{
	return (a->mask & b->layer) != 0 && (b->mask & a->layer) != 0;
}

static int world_reserve(bapi_collision_world_t world, int needed)
{
	if (needed <= world->capacity) return 1;

	int capacity = world->capacity > 0 ? world->capacity : 8;
	while (capacity < needed) capacity *= 2;

	bapi_collider_t *colliders =
		(bapi_collider_t *)realloc(world->colliders, (size_t)capacity * sizeof(*colliders));
	if (colliders == NULL) return 0;

	world->colliders = colliders;
	world->capacity	 = capacity;
	return 1;
}

/* Sweep `self` along one axis by `amount`, stopping at the nearest solid
 * collider that passes the filter. Colliders behind the leading edge are
 * ignored, so a single large delta cannot tunnel through thin obstacles.
 * Returns the number of colliders that actually limited the movement. */
static int sweep_axis(bapi_collision_world_t world, bapi_collider_t self, int horizontal,
					  float amount)
{
	if (!(amount > 0.0f) && !(amount < 0.0f)) return 0;

	const float start	 = horizontal ? self->bounds.x : self->bounds.y;
	const float size	 = horizontal ? self->bounds.w : self->bounds.h;
	const float target	 = start + amount;
	float		limit	 = target;
	int			contacts = 0;

	for (int i = 0; i < world->count; i++) {
		bapi_collider_t other = world->colliders[i];
		if (other == self || !other->solid) continue;
		if (!filter_pass(self, other)) continue;

		if (horizontal) {
			if (!spans_overlap(self->bounds.y, self->bounds.y + self->bounds.h, other->bounds.y,
							   other->bounds.y + other->bounds.h))
				continue;
		} else {
			if (!spans_overlap(self->bounds.x, self->bounds.x + self->bounds.w, other->bounds.x,
							   other->bounds.x + other->bounds.w))
				continue;
		}

		if (amount > 0.0f) {
			const float other_start = horizontal ? other->bounds.x : other->bounds.y;
			const float facing_edge = start + size;

			/* Already behind (or overlapping) the trailing edge: not a new
			 * contact. */
			if (other_start < facing_edge - COLLISION_EPSILON) continue;
			/* The leading edge stops before this collider: no contact. */
			if (other_start >= target + size) continue;

			const float candidate = other_start - size - COLLISION_EPSILON;
			if (candidate < limit) {
				limit = candidate;
				contacts++;
			}
		} else {
			const float other_end =
				horizontal ? other->bounds.x + other->bounds.w : other->bounds.y + other->bounds.h;

			/* The leading edge stops after this collider: no contact. */
			if (other_end > start + COLLISION_EPSILON) continue;
			if (other_end <= target) continue;

			const float candidate = other_end + COLLISION_EPSILON;
			if (candidate > limit) {
				limit = candidate;
				contacts++;
			}
		}
	}

	/* Never push the collider backwards past where it started. */
	if (amount > 0.0f) {
		if (limit < start) limit = start;
	} else {
		if (limit > start) limit = start;
	}

	if (horizontal) {
		self->bounds.x = limit;
	} else {
		self->bounds.y = limit;
	}
	return contacts;
}

bapi_collision_world_t bapi_collision_world_create(void)
{
	bapi_collision_world_t world =
		(bapi_collision_world_t)calloc(1, sizeof(struct bapi_collision_world_internal));
	return world;
}

void bapi_collision_world_destroy(bapi_collision_world_t world)
{
	if (world == NULL) return;

	for (int i = 0; i < world->count; i++) {
		free(world->colliders[i]);
	}
	free(world->colliders);
	free(world);
}

bapi_collider_t bapi_collision_world_add(bapi_collision_world_t world, bapi_rect_t bounds,
										 uint32_t layer, uint32_t mask, int solid)
{
	if (world == NULL) return NULL;

	bapi_collider_t collider = (bapi_collider_t)malloc(sizeof(struct bapi_collider_internal));
	if (collider == NULL) return NULL;

	collider->bounds = bounds;
	collider->layer	 = layer;
	collider->mask	 = mask;
	collider->solid	 = solid != 0;

	if (!world_reserve(world, world->count + 1)) {
		free(collider);
		return NULL;
	}

	world->colliders[world->count++] = collider;
	return collider;
}

void bapi_collision_world_remove(bapi_collision_world_t world, bapi_collider_t collider)
{
	if (world == NULL || collider == NULL) return;

	for (int i = 0; i < world->count; i++) {
		if (world->colliders[i] != collider) continue;

		free(collider);
		for (int j = i; j < world->count - 1; j++) {
			world->colliders[j] = world->colliders[j + 1];
		}
		world->count--;
		return;
	}
}

void bapi_collider_set_bounds(bapi_collider_t collider, bapi_rect_t bounds)
{
	if (collider == NULL) return;
	collider->bounds = bounds;
}

bapi_rect_t bapi_collider_get_bounds(bapi_collider_t collider)
{
	if (collider == NULL) return (bapi_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return collider->bounds;
}

void bapi_collider_set_layer_mask(bapi_collider_t collider, uint32_t layer, uint32_t mask)
{
	if (collider == NULL) return;
	collider->layer = layer;
	collider->mask	= mask;
}

/* Returns the total number of colliders that overlap `bounds` and whose layer
 * intersects `layer` (0 matches every layer). Up to `max` handles are written
 * to `out`; the return value may therefore exceed `max`. Negative or zero
 * `max`, or a NULL `out`, only count. */
int bapi_collision_world_query(bapi_collision_world_t world, bapi_rect_t bounds, uint32_t layer,
							   bapi_collider_t *out, int max)
{
	if (world == NULL) return 0;

	int found = 0;
	for (int i = 0; i < world->count; i++) {
		bapi_collider_t collider = world->colliders[i];
		if (layer != 0 && (collider->layer & layer) == 0) continue;
		if (!rects_overlap(bounds, collider->bounds)) continue;

		if (out != NULL && found < max) out[found] = collider;
		found++;
	}
	return found;
}

/* Moves X first, then Y, against solid colliders that pass the layer/mask
 * filter. Sets *hit to 1 when at least one collider blocked the movement and
 * returns the number of blocking contacts. */
int bapi_collision_world_move(bapi_collision_world_t world, bapi_collider_t collider,
							  bapi_vec2_t delta, int *hit)
{
	if (hit != NULL) *hit = 0;
	if (world == NULL || collider == NULL) return 0;

	int contacts = 0;
	contacts += sweep_axis(world, collider, 1, delta.x);
	contacts += sweep_axis(world, collider, 0, delta.y);

	if (contacts > 0 && hit != NULL) *hit = 1;
	return contacts;
}
