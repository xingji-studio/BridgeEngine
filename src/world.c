#include "BridgeEngine.h"

#include <stdlib.h>
#include <string.h>

struct bapi_world_internal {
	bapi_entity_t *roots;
	int			   root_count;
	int			   root_capacity;

	/* Entities whose destruction was requested while an update is running.
	 * Destruction is deferred to the end of bapi_world_update so update
	 * callbacks may safely despawn entities (including themselves). */
	bapi_entity_t *pending;
	int			   pending_count;
	int			   pending_capacity;

	int			  updating;
	int			  paused;
	bapi_camera_t camera;
};

static int world_grow_roots(bapi_world_t world, int needed)
{
	if (world->root_capacity >= needed) return 0;

	int			   new_capacity = world->root_capacity == 0 ? 8 : world->root_capacity * 2;
	bapi_entity_t *grown;
	while (new_capacity < needed) new_capacity *= 2;

	grown = realloc(world->roots, (size_t)new_capacity * sizeof(*grown));
	if (grown == NULL) return -1;
	world->roots		 = grown;
	world->root_capacity = new_capacity;
	return 0;
}

static int world_grow_pending(bapi_world_t world, int needed)
{
	if (world->pending_capacity >= needed) return 0;

	int			   new_capacity = world->pending_capacity == 0 ? 8 : world->pending_capacity * 2;
	bapi_entity_t *grown;
	while (new_capacity < needed) new_capacity *= 2;

	grown = realloc(world->pending, (size_t)new_capacity * sizeof(*grown));
	if (grown == NULL) return -1;
	world->pending			= grown;
	world->pending_capacity = new_capacity;
	return 0;
}

static int world_root_index(bapi_world_t world, bapi_entity_t entity)
{
	for (int i = 0; i < world->root_count; i++) {
		if (world->roots[i] == entity) return i;
	}
	return -1;
}

static void world_remove_root_at(bapi_world_t world, int index)
{
	if (index < 0 || index >= world->root_count) return;
	for (int i = index; i + 1 < world->root_count; i++) {
		world->roots[i] = world->roots[i + 1];
	}
	world->root_count--;
}

/* True when `candidate` is `root` or one of its descendants. The comparison
 * only walks live entities, so callers must invoke this before destroying
 * `root`. */
static int world_entity_in_subtree(bapi_entity_t root, bapi_entity_t candidate)
{
	if (root == NULL || candidate == NULL) return 0;
	if (root == candidate) return 1;
	int children = bapi_entity_get_child_count(root);
	for (int i = 0; i < children; i++) {
		if (world_entity_in_subtree(bapi_entity_get_child(root, i), candidate)) return 1;
	}
	return 0;
}

static int world_pending_index(bapi_world_t world, bapi_entity_t entity)
{
	for (int i = 0; i < world->pending_count; i++) {
		if (world->pending[i] == entity) return i;
	}
	return -1;
}

static void world_queue_pending(bapi_world_t world, bapi_entity_t entity)
{
	/* Duplicate requests are harmless: keep exactly one queue entry. */
	if (world_pending_index(world, entity) >= 0) return;
	if (world_grow_pending(world, world->pending_count + 1) != 0) {
		/* Out of memory. Never free an entity while its own update callback is
		 * still running; outside an update immediate destruction is safe. */
		if (!world->updating) bapi_entity_destroy(entity);
		return;
	}
	world->pending[world->pending_count++] = entity;
}

/* Destroying an entity frees its whole subtree. Drop queued pointers that
 * are part of that subtree so the flush never touches them again. */
static void world_pending_forget_subtree(bapi_world_t world, int from_index, bapi_entity_t entity)
{
	for (int i = from_index; i < world->pending_count; i++) {
		if (world->pending[i] != NULL && world_entity_in_subtree(entity, world->pending[i])) {
			world->pending[i] = NULL;
		}
	}
}

static void world_flush_pending(bapi_world_t world)
{
	if (world->pending_count == 0) return;

	for (int i = 0; i < world->pending_count; i++) {
		bapi_entity_t entity = world->pending[i];
		if (entity == NULL) continue;

		/* Descendants of this entity were queued too; forget them before the
		 * subtree is freed. */
		world_pending_forget_subtree(world, i + 1, entity);
		int root_index = world_root_index(world, entity);
		if (root_index >= 0) world_remove_root_at(world, root_index);
		bapi_entity_destroy(entity);
	}

	world->pending_count = 0;
}

bapi_world_t bapi_world_create(void)
{
	bapi_world_t world = calloc(1, sizeof(*world));
	if (world == NULL) return NULL;

	world->camera.zoom = 1.0f;
	return world;
}

void bapi_world_destroy(bapi_world_t world)
{
	if (world == NULL) return;

	/* Entities queued for destruction no longer belong to the root list, so
	 * flush them before tearing the roots down. */
	world_flush_pending(world);

	while (world->root_count > 0) {
		bapi_entity_destroy(world->roots[world->root_count - 1]);
		world->root_count--;
	}
	free(world->roots);
	free(world->pending);
	free(world);
}

bapi_entity_t bapi_world_spawn(bapi_world_t world, const char *name)
{
	if (world == NULL) return NULL;

	bapi_entity_t entity = bapi_entity_create(name);
	if (entity == NULL) return NULL;

	/* Spawned entities are roots. A parented entity is never produced here,
	 * but if one ever arrives, adopting it as a root would double-own it:
	 * keep it alive and let its current parent remain the owner. */
	if (bapi_entity_get_parent(entity) != NULL) return entity;

	if (world_grow_roots(world, world->root_count + 1) != 0) {
		bapi_entity_destroy(entity);
		return NULL;
	}
	world->roots[world->root_count++] = entity;
	return entity;
}

void bapi_world_despawn(bapi_world_t world, bapi_entity_t entity)
{
	if (world == NULL || entity == NULL) return;

	/* Stop updating/drawing immediately, but keep the memory alive until the
	 * current update finishes so callbacks may safely continue. */
	int root_index = world_root_index(world, entity);
	if (root_index >= 0) world_remove_root_at(world, root_index);

	/* A child is detached from its parent right away; the parent must not see
	 * (or later free) an entity that is already queued for destruction. */
	bapi_entity_t parent = bapi_entity_get_parent(entity);
	if (parent != NULL) bapi_entity_remove_child(parent, entity);

	world_queue_pending(world, entity);
}

void bapi_world_update(bapi_world_t world, float delta_time)
{
	if (world == NULL || world->paused) return;
	if (world->root_count == 0) {
		world_flush_pending(world);
		return;
	}

	/* Snapshot the roots: callbacks may spawn or despawn entities while the
	 * loop runs. Newly spawned roots start updating next frame. */
	int			   count	= world->root_count;
	bapi_entity_t *snapshot = malloc((size_t)count * sizeof(*snapshot));
	if (snapshot != NULL) {
		memcpy(snapshot, world->roots, (size_t)count * sizeof(*snapshot));
	} else {
		/* Allocation failed: fall back to index iteration below. */
		count = 0;
	}

	world->updating = 1;
	if (snapshot != NULL) {
		for (int i = 0; i < count; i++) {
			/* A root despawned earlier in this update must not run its callback. */
			if (world_root_index(world, snapshot[i]) < 0) continue;
			bapi_entity_update(snapshot[i], delta_time);
		}
	} else {
		for (int i = 0; i < world->root_count; i++) {
			bapi_entity_update(world->roots[i], delta_time);
			if (i >= world->root_count) break;
		}
	}
	world->updating = 0;

	world_flush_pending(world);
	free(snapshot);
}

void bapi_world_draw(bapi_world_t world)
{
	if (world == NULL) return;

	bapi_apply_camera(&world->camera);
	for (int i = 0; i < world->root_count; i++) {
		bapi_entity_draw(world->roots[i]);
	}
	bapi_reset_transform();
}

void bapi_world_set_camera(bapi_world_t world, bapi_camera_t camera)
{
	if (world != NULL) world->camera = camera;
}

void bapi_world_get_camera(bapi_world_t world, bapi_camera_t *out_camera)
{
	if (out_camera == NULL) return;
	if (world != NULL) {
		*out_camera = world->camera;
	} else {
		*out_camera = (bapi_camera_t){0};
	}
}

int bapi_world_entity_count(bapi_world_t world)
{
	return world != NULL ? world->root_count : 0;
}

bapi_entity_t bapi_world_get_entity(bapi_world_t world, int index)
{
	if (world == NULL || index < 0 || index >= world->root_count) return NULL;
	return world->roots[index];
}

void bapi_world_set_paused(bapi_world_t world, int paused)
{
	if (world != NULL) world->paused = paused != 0;
}

int bapi_world_is_paused(bapi_world_t world)
{
	return world != NULL ? world->paused : 0;
}
