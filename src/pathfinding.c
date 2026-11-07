/* Grid A*. Path excludes the start tile and includes the goal tile; solid
 * tiles are blocked. Diagonals use octile cost and do not check corner cutting.
 */

#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/tilemap_internal.h"

#include <stdint.h>
#include <stdlib.h>

#define BAPI_PATH_SQRT2	  1.41421356f
#define BAPI_PATH_INF	  1.0e30f
#define BAPI_PATH_EPSILON 1.0e-5f

struct bapi_path_internal {
	int	 count;
	int *xs;
	int *ys;
};

typedef struct {
	int	  node;
	float f;
	float h;
} path_heap_entry_t;

typedef struct {
	path_heap_entry_t *items;
	int				   count;
	int				   capacity;
} path_heap_t;

/* Fixed expansion order keeps results reproducible. */
static const int k_directions4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
static const int k_directions8[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
										{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/* Entries freeze f/h at push time: lazy-deletion heaps must not read scores
 * that change after insertion, or the heap invariant can be violated. Ties are
 * broken by h and then by node index so the result is deterministic. */
static int path_entry_less(const path_heap_entry_t *a, const path_heap_entry_t *b)
{
	if (a->f != b->f) return a->f < b->f;
	if (a->h != b->h) return a->h < b->h;
	return a->node < b->node;
}

/* Lazily-deleted binary min-heap. Duplicate entries are resolved by the caller
 * through its closed set. */
static int path_heap_push(path_heap_t *heap, int node, float f, float h)
{
	if (heap->count == heap->capacity) {
		int				   new_capacity = heap->capacity == 0 ? 64 : heap->capacity * 2;
		path_heap_entry_t *items = realloc(heap->items, (size_t)new_capacity * sizeof(*items));
		if (items == NULL) return -1;
		heap->items	   = items;
		heap->capacity = new_capacity;
	}

	path_heap_entry_t entry = {node, f, h};
	int				  index = heap->count++;
	while (index > 0) {
		int parent = (index - 1) / 2;
		if (!path_entry_less(&entry, &heap->items[parent])) break;
		heap->items[index] = heap->items[parent];
		index			   = parent;
	}
	heap->items[index] = entry;
	return 0;
}

static int path_heap_pop(path_heap_t *heap)
{
	int				  top  = heap->items[0].node;
	path_heap_entry_t last = heap->items[--heap->count];
	if (heap->count == 0) return top;

	/* Replace the root with the last entry and sift it down. */
	heap->items[0] = last;
	int index	   = 0;
	while (1) {
		int left	 = index * 2 + 1;
		int right	 = left + 1;
		int smallest = index;

		if (left < heap->count && path_entry_less(&heap->items[left], &heap->items[smallest])) {
			smallest = left;
		}
		if (right < heap->count && path_entry_less(&heap->items[right], &heap->items[smallest])) {
			smallest = right;
		}
		if (smallest == index) break;

		path_heap_entry_t swap = heap->items[index];
		heap->items[index]	   = heap->items[smallest];
		heap->items[smallest]  = swap;
		index				   = smallest;
	}

	return top;
}

static float path_heuristic(int dx, int dy, int allow_diagonal)
{
	int ax = dx < 0 ? -dx : dx;
	int ay = dy < 0 ? -dy : dy;

	if (!allow_diagonal) return (float)(ax + ay);

	int min = ax < ay ? ax : ay;
	int max = ax < ay ? ay : ax;
	return (float)(max - min) + BAPI_PATH_SQRT2 * (float)min;
}

static void path_free_search(float *g_score, float *f_score, float *h_score, int *parent,
							 uint8_t *state, path_heap_t *heap)
{
	free(g_score);
	free(f_score);
	free(h_score);
	free(parent);
	free(state);
	free(heap->items);
}

bapi_path_t bapi_path_find(bapi_tilemap_t map, int start_x, int start_y, int goal_x, int goal_y,
						   int allow_diagonal)
{
	if (map == NULL) {
		bapi_set_last_error("bapi_path_find: tilemap is NULL");
		return NULL;
	}
	if (map->width <= 0 || map->height <= 0 || map->solid == NULL) {
		bapi_set_last_error("bapi_path_find: tilemap has no grid");
		return NULL;
	}
	if (start_x < 0 || start_y < 0 || start_x >= map->width || start_y >= map->height) {
		bapi_set_last_error("bapi_path_find: start is outside the grid");
		return NULL;
	}
	if (goal_x < 0 || goal_y < 0 || goal_x >= map->width || goal_y >= map->height) {
		bapi_set_last_error("bapi_path_find: goal is outside the grid");
		return NULL;
	}
	if (bapi_tilemap_is_solid(map, start_x, start_y)) {
		bapi_set_last_error("bapi_path_find: start tile is solid");
		return NULL;
	}
	if (bapi_tilemap_is_solid(map, goal_x, goal_y)) {
		bapi_set_last_error("bapi_path_find: goal tile is solid");
		return NULL;
	}

	const int width	 = map->width;
	const int height = map->height;
	const int count	 = width * height;

	bapi_path_t path = calloc(1, sizeof(*path));
	if (path == NULL) return NULL;

	int start_node = start_y * width + start_x;
	int goal_node  = goal_y * width + goal_x;

	if (start_node == goal_node) return path; /* empty path, count stays 0 */

	float	   *g_score = malloc((size_t)count * sizeof(float));
	float	   *f_score = malloc((size_t)count * sizeof(float));
	float	   *h_score = malloc((size_t)count * sizeof(float));
	int		   *parent	= malloc((size_t)count * sizeof(int));
	uint8_t	   *state	= calloc((size_t)count, sizeof(uint8_t));
	path_heap_t heap	= {NULL, 0, 0};
	if (g_score == NULL || f_score == NULL || h_score == NULL || parent == NULL || state == NULL) {
		path_free_search(g_score, f_score, h_score, parent, state, &heap);
		free(path);
		bapi_set_last_error("bapi_path_find: out of memory");
		return NULL;
	}

	for (int i = 0; i < count; i++) {
		g_score[i] = BAPI_PATH_INF;
		f_score[i] = BAPI_PATH_INF;
		h_score[i] = 0.0f;
		parent[i]  = -1;
	}

	const int diagonal_count   = allow_diagonal ? 8 : 4;
	const int (*directions)[2] = allow_diagonal ? k_directions8 : k_directions4;

	g_score[start_node] = 0.0f;
	h_score[start_node] = path_heuristic(goal_x - start_x, goal_y - start_y, allow_diagonal);
	f_score[start_node] = h_score[start_node];
	state[start_node]	= 1; /* open */
	if (path_heap_push(&heap, start_node, f_score[start_node], h_score[start_node]) != 0) {
		path_free_search(g_score, f_score, h_score, parent, state, &heap);
		free(path);
		bapi_set_last_error("bapi_path_find: out of memory");
		return NULL;
	}

	int found = 0;
	while (heap.count > 0) {
		int current = path_heap_pop(&heap);
		if (state[current] == 2) continue; /* stale duplicate entry */
		state[current] = 2;				   /* closed */

		if (current == goal_node) {
			found = 1;
			break;
		}

		int current_x = current % width;
		int current_y = current / width;

		for (int direction = 0; direction < diagonal_count; direction++) {
			int next_x = current_x + directions[direction][0];
			int next_y = current_y + directions[direction][1];
			if (next_x < 0 || next_y < 0 || next_x >= width || next_y >= height) continue;

			int next_node = next_y * width + next_x;
			if (state[next_node] == 2) continue;
			if (map->solid[next_node]) continue;

			int	  diagonal	= directions[direction][0] != 0 && directions[direction][1] != 0;
			float step_cost = diagonal ? BAPI_PATH_SQRT2 : 1.0f;
			float tentative = g_score[current] + step_cost;
			if (tentative >= g_score[next_node] - BAPI_PATH_EPSILON) continue;

			g_score[next_node] = tentative;
			parent[next_node]  = current;
			state[next_node]   = 1;
			float h			   = path_heuristic(goal_x - next_x, goal_y - next_y, allow_diagonal);
			h_score[next_node] = h;
			f_score[next_node] = tentative + h;
			if (path_heap_push(&heap, next_node, f_score[next_node], h_score[next_node]) != 0) {
				path_free_search(g_score, f_score, h_score, parent, state, &heap);
				free(path);
				bapi_set_last_error("bapi_path_find: out of memory");
				return NULL;
			}
		}
	}

	if (!found) {
		path_free_search(g_score, f_score, h_score, parent, state, &heap);
		free(path);
		return NULL; /* unreachable */
	}

	/* Count steps from the goal back to the start; the start is excluded. */
	int steps = 0;
	for (int node = goal_node; node != start_node; node = parent[node]) {
		if (node < 0) {
			/* Defensive: a broken parent chain must never be published. */
			path_free_search(g_score, f_score, h_score, parent, state, &heap);
			free(path);
			return NULL;
		}
		steps++;
	}

	path->count = steps;
	if (steps > 0) {
		path->xs = malloc((size_t)steps * sizeof(int));
		path->ys = malloc((size_t)steps * sizeof(int));
		if (path->xs == NULL || path->ys == NULL) {
			free(path->xs);
			free(path->ys);
			path_free_search(g_score, f_score, h_score, parent, state, &heap);
			free(path);
			bapi_set_last_error("bapi_path_find: out of memory");
			return NULL;
		}
	}

	/* Walk backwards so the path runs start -> goal, excluding the start. */
	int node = goal_node;
	for (int index = steps - 1; index >= 0; index--) {
		path->xs[index] = node % width;
		path->ys[index] = node / width;
		node			= parent[node];
	}

	path_free_search(g_score, f_score, h_score, parent, state, &heap);
	return path;
}

void bapi_path_destroy(bapi_path_t path)
{
	if (path == NULL) return;
	free(path->xs);
	free(path->ys);
	free(path);
}

int bapi_path_count(bapi_path_t path)
{
	if (path == NULL) return 0;
	return path->count;
}

int bapi_path_get(bapi_path_t path, int index, int *x, int *y)
{
	if (path == NULL) return -1;
	if (index < 0 || index >= path->count) return -1;
	if (x != NULL) *x = path->xs[index];
	if (y != NULL) *y = path->ys[index];
	return 0;
}
