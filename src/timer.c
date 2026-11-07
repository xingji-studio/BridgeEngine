#include "BridgeEngine.h"

#include <limits.h>
#include <stdlib.h>

typedef struct timer_node {
	struct timer_node *next;
	int				   id;
	float			   remaining;
	float			   interval;
	int				   repeating;
	int				   alive;
	bapi_timer_fn	   callback;
	void			  *user_data;
} timer_node_t;

struct bapi_timer_manager_internal {
	timer_node_t *head;
	timer_node_t *tail;
	int			  next_id;
	int			  count;
	int			  updating;
};

/* Remove every dead node from the list. Nodes are only unlinked here, never
 * while callbacks are running, so callbacks may safely cancel timers (their
 * own included) without invalidating the update iteration. */
static void timer_sweep(bapi_timer_manager_t manager)
{
	timer_node_t **link = &manager->head;
	timer_node_t  *tail = NULL;

	while (*link != NULL) {
		timer_node_t *node = *link;
		if (!node->alive) {
			*link = node->next;
			free(node);
			continue;
		}
		tail = node;
		link = &node->next;
	}
	manager->tail = tail;
}

static timer_node_t *timer_find(bapi_timer_manager_t manager, int timer_id)
{
	if (timer_id <= 0) return NULL;
	for (timer_node_t *node = manager->head; node != NULL; node = node->next) {
		if (node->alive && node->id == timer_id) return node;
	}
	return NULL;
}

static int timer_next_id(bapi_timer_manager_t manager)
{
	/* IDs stay positive and unique among live timers. */
	for (;;) {
		int id = manager->next_id;
		if (id <= 0 || id == INT_MAX) {
			id				 = 1;
			manager->next_id = 2;
		} else {
			manager->next_id = id + 1;
		}
		if (timer_find(manager, id) == NULL) return id;
	}
}

static int timer_add(bapi_timer_manager_t manager, float seconds, int repeating, float interval,
					 bapi_timer_fn callback, void *user_data)
{
	if (manager == NULL || callback == NULL) return -1;
	if (!(seconds > 0.0f)) return -1;
	if (repeating && !(interval > 0.0f)) return -1;

	timer_node_t *node = calloc(1, sizeof(*node));
	if (node == NULL) return -1;

	node->id		= timer_next_id(manager);
	node->remaining = seconds;
	node->interval	= repeating ? interval : 0.0f;
	node->repeating = repeating;
	node->alive		= 1;
	node->callback	= callback;
	node->user_data = user_data;

	if (manager->tail != NULL) {
		manager->tail->next = node;
	} else {
		manager->head = node;
	}
	manager->tail = node;
	manager->count++;
	return node->id;
}

bapi_timer_manager_t bapi_timer_manager_create(void)
{
	bapi_timer_manager_t manager = calloc(1, sizeof(*manager));
	if (manager == NULL) return NULL;
	manager->next_id = 1;
	return manager;
}

void bapi_timer_manager_destroy(bapi_timer_manager_t manager)
{
	if (manager == NULL) return;

	timer_node_t *node = manager->head;
	while (node != NULL) {
		timer_node_t *next = node->next;
		free(node);
		node = next;
	}
	free(manager);
}

int bapi_timer_after(bapi_timer_manager_t manager, float seconds, bapi_timer_fn callback,
					 void *user_data)
{
	return timer_add(manager, seconds, 0, 0.0f, callback, user_data);
}

int bapi_timer_every(bapi_timer_manager_t manager, float interval, bapi_timer_fn callback,
					 void *user_data)
{
	return timer_add(manager, interval, 1, interval, callback, user_data);
}

void bapi_timer_cancel(bapi_timer_manager_t manager, int timer_id)
{
	if (manager == NULL) return;

	timer_node_t *node = timer_find(manager, timer_id);
	if (node == NULL) return;

	node->alive = 0;
	manager->count--;
	if (!manager->updating) timer_sweep(manager);
}

void bapi_timer_manager_update(bapi_timer_manager_t manager, float delta_time)
{
	if (manager == NULL || !(delta_time > 0.0f)) return;
	if (manager->head == NULL) return;

	/* Timers added from callbacks are appended at the tail and must not fire
	 * before the next update. The list is not compacted while iterating, so
	 * cancelling a timer only flags it: the sweep runs afterwards. */
	int			  initial_count = manager->count;
	timer_node_t *node			= manager->head;

	manager->updating = 1;
	for (int i = 0; i < initial_count && node != NULL; i++) {
		timer_node_t *next = node->next;
		if (!node->alive) {
			node = next;
			continue;
		}

		node->remaining -= delta_time;
		if (node->remaining <= 0.0f) {
			if (node->repeating) {
				/* Reschedule before firing so a callback sees a coherent timer.
				 * Missed whole intervals are skipped rather than fired in a burst. */
				if (node->interval > 0.0f) {
					int guard = 0;
					do {
						node->remaining += node->interval;
						guard++;
					} while (node->remaining <= 0.0f && guard < 1000000);
					if (!(node->remaining > 0.0f)) node->remaining = node->interval;
				}
			} else {
				/* Mark dead before the callback so a self-cancel is a no-op. */
				node->alive = 0;
				manager->count--;
			}

			node->callback(node->user_data);
		}
		node = next;
	}
	manager->updating = 0;

	timer_sweep(manager);
}

int bapi_timer_count(bapi_timer_manager_t manager)
{
	return manager != NULL ? manager->count : 0;
}
