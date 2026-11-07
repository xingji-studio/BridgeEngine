#include "BridgeEngine.h"

#include <stdio.h>
#include <string.h>

static bapi_timer_manager_t g_manager;

static const int g_index_values[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static int		 g_fires[8];
static int		 g_order[16];
static int		 g_order_count;

static int g_self_id;
static int g_cancel_target;
static int g_added_id;
static int g_add_count;

static void count_fire(void *user_data)
{
	int index = *(const int *)user_data;
	if (index >= 0 && index < 8) g_fires[index]++;
	if (g_order_count < (int)(sizeof(g_order) / sizeof(g_order[0]))) {
		g_order[g_order_count++] = index;
	}
}

static void cancel_other(void *user_data)
{
	(void)user_data;
	count_fire(user_data);
	bapi_timer_cancel(g_manager, g_cancel_target);
}

static void self_cancel(void *user_data)
{
	(void)user_data;
	count_fire((void *)&g_index_values[7]);
	bapi_timer_cancel(g_manager, g_self_id);
}

static void add_timer_from_callback(void *user_data)
{
	(void)user_data;
	count_fire((void *)&g_index_values[6]);
	g_added_id = bapi_timer_after(g_manager, 0.5f, count_fire, (void *)&g_index_values[5]);
	g_add_count++;
}

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static void reset_counters(void)
{
	memset(g_fires, 0, sizeof(g_fires));
	memset(g_order, 0, sizeof(g_order));
	g_order_count	= 0;
	g_self_id		= 0;
	g_cancel_target = 0;
	g_added_id		= 0;
	g_add_count		= 0;
}

static int test_one_shot(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);
	CHECK(bapi_timer_count(manager) == 0);

	int idx = 0;
	int id	= bapi_timer_after(manager, 1.0f, count_fire, &idx);
	CHECK(id > 0);
	CHECK(bapi_timer_count(manager) == 1);

	/* Not due yet. */
	bapi_timer_manager_update(manager, 0.25f);
	CHECK(g_fires[0] == 0);
	CHECK(bapi_timer_count(manager) == 1);

	bapi_timer_manager_update(manager, 0.75f);
	CHECK(g_fires[0] == 1);
	CHECK(bapi_timer_count(manager) == 0);

	/* A one-shot never fires again. */
	bapi_timer_manager_update(manager, 10.0f);
	CHECK(g_fires[0] == 1);
	CHECK(bapi_timer_count(manager) == 0);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_repeating_and_cancel(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	int idx = 1;
	int id	= bapi_timer_every(manager, 1.0f, count_fire, &idx);
	CHECK(id > 0);
	CHECK(bapi_timer_count(manager) == 1);

	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[1] == 1);
	CHECK(bapi_timer_count(manager) == 1);

	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[1] == 2);
	CHECK(bapi_timer_count(manager) == 1);

	bapi_timer_manager_update(manager, 0.5f);
	CHECK(g_fires[1] == 2);
	bapi_timer_manager_update(manager, 0.5f);
	CHECK(g_fires[1] == 3);

	bapi_timer_cancel(manager, id);
	CHECK(bapi_timer_count(manager) == 0);
	bapi_timer_manager_update(manager, 5.0f);
	CHECK(g_fires[1] == 3);
	CHECK(bapi_timer_count(manager) == 0);

	/* Cancelling an unknown/expired id is a no-op. */
	bapi_timer_cancel(manager, id);
	bapi_timer_cancel(manager, 0);
	bapi_timer_cancel(manager, -7);
	CHECK(bapi_timer_count(manager) == 0);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_algorithm_details(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	/* A large delta fires a repeating timer once and skips missed intervals. */
	int idx = 2;
	bapi_timer_every(manager, 1.0f, count_fire, &idx);
	bapi_timer_manager_update(manager, 2.5f);
	CHECK(g_fires[2] == 1);
	/* Phase is preserved: next fire is 0.5s away. */
	bapi_timer_manager_update(manager, 0.25f);
	CHECK(g_fires[2] == 1);
	bapi_timer_manager_update(manager, 0.25f);
	CHECK(g_fires[2] == 2);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_multiple_timers_and_ids(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	int first  = bapi_timer_after(manager, 0.5f, count_fire, (void *)&g_index_values[0]);
	int second = bapi_timer_after(manager, 1.0f, count_fire, (void *)&g_index_values[1]);
	int third  = bapi_timer_after(manager, 1.0f, count_fire, (void *)&g_index_values[2]);
	CHECK(first > 0 && second > 0 && third > 0);
	CHECK(first != second && second != third && first != third);
	CHECK(bapi_timer_count(manager) == 3);

	/* Same deadline: insertion order. */
	g_order_count = 0;
	bapi_timer_manager_update(manager, 0.5f);
	CHECK(g_fires[0] == 1);
	CHECK(g_fires[1] == 0 && g_fires[2] == 0);
	CHECK(bapi_timer_count(manager) == 2);

	g_order_count = 0;
	bapi_timer_manager_update(manager, 0.5f);
	CHECK(g_fires[1] == 1 && g_fires[2] == 1);
	CHECK(g_order_count == 2);
	CHECK(g_order[0] == 1 && g_order[1] == 2);
	CHECK(bapi_timer_count(manager) == 0);

	/* IDs are not reused for live timers; fresh IDs are positive. */
	int fourth = bapi_timer_after(manager, 1.0f, count_fire, (void *)&g_index_values[3]);
	CHECK(fourth > 0);
	CHECK(fourth != first && fourth != second && fourth != third);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_cancel_and_dt_edge_cases(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	/* dt <= 0 does not advance anything. */
	int idx = 3;
	int id	= bapi_timer_after(manager, 1.0f, count_fire, &idx);
	CHECK(id > 0);
	bapi_timer_manager_update(manager, 0.0f);
	bapi_timer_manager_update(manager, -3.0f);
	CHECK(g_fires[3] == 0);
	CHECK(bapi_timer_count(manager) == 1);
	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[3] == 1);

	/* Cancel removes a pending timer. */
	int idx_a = 0;
	int idx_b = 1;
	int id_a  = bapi_timer_after(manager, 1.0f, count_fire, &idx_a);
	int id_b  = bapi_timer_after(manager, 1.0f, count_fire, &idx_b);
	CHECK(id_a > 0 && id_b > 0);
	bapi_timer_cancel(manager, id_a);
	CHECK(bapi_timer_count(manager) == 1);
	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[0] == 0);
	CHECK(g_fires[1] == 1);
	CHECK(bapi_timer_count(manager) == 0);

	/* Invalid scheduling arguments. */
	CHECK(bapi_timer_after(manager, 0.0f, count_fire, NULL) == -1);
	CHECK(bapi_timer_after(manager, -1.0f, count_fire, NULL) == -1);
	CHECK(bapi_timer_after(manager, 1.0f, NULL, NULL) == -1);
	CHECK(bapi_timer_after(NULL, 1.0f, count_fire, NULL) == -1);
	CHECK(bapi_timer_every(manager, 0.0f, count_fire, NULL) == -1);
	CHECK(bapi_timer_every(manager, -2.0f, count_fire, NULL) == -1);
	CHECK(bapi_timer_every(manager, 1.0f, NULL, NULL) == -1);
	CHECK(bapi_timer_every(NULL, 1.0f, count_fire, NULL) == -1);

	/* NULL-safe entry points. */
	CHECK(bapi_timer_count(NULL) == 0);
	bapi_timer_cancel(NULL, 1);
	bapi_timer_manager_update(NULL, 1.0f);
	bapi_timer_manager_destroy(NULL);
	CHECK(bapi_timer_count(manager) == 0);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_cancel_from_callback(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	/* A is created first, so it fires first and cancels B before B's turn even
	 * though both are due in the same update. */
	int id_a		= bapi_timer_after(manager, 1.0f, cancel_other, (void *)&g_index_values[0]);
	g_cancel_target = bapi_timer_after(manager, 1.0f, count_fire, (void *)&g_index_values[1]);
	CHECK(g_cancel_target > 0 && id_a > 0);
	CHECK(bapi_timer_count(manager) == 2);

	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[0] == 1);
	CHECK(g_fires[1] == 0);
	CHECK(bapi_timer_count(manager) == 0);

	/* Repeating timer cancels itself from its own callback. */
	int id_self = bapi_timer_every(manager, 1.0f, self_cancel, NULL);
	CHECK(id_self > 0);
	g_self_id = id_self;
	CHECK(bapi_timer_count(manager) == 1);
	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[7] == 1);
	CHECK(bapi_timer_count(manager) == 0);
	bapi_timer_manager_update(manager, 5.0f);
	CHECK(g_fires[7] == 1);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_timer_added_from_callback(void)
{
	reset_counters();
	g_manager					 = bapi_timer_manager_create();
	bapi_timer_manager_t manager = g_manager;
	CHECK(manager != NULL);

	int id = bapi_timer_after(manager, 1.0f, add_timer_from_callback, NULL);
	CHECK(id > 0);

	bapi_timer_manager_update(manager, 1.0f);
	CHECK(g_fires[6] == 1);
	CHECK(g_add_count == 1);
	CHECK(g_added_id > 0);
	CHECK(bapi_timer_count(manager) == 1);
	/* A timer added during update must not fire before the next update. */
	CHECK(g_fires[5] == 0);

	bapi_timer_manager_update(manager, 0.5f);
	CHECK(g_fires[5] == 1);
	CHECK(bapi_timer_count(manager) == 0);

	bapi_timer_manager_destroy(manager);
	return 0;
}

static int test_destroy_does_not_fire(void)
{
	reset_counters();
	bapi_timer_manager_t manager = bapi_timer_manager_create();
	CHECK(manager != NULL);

	CHECK(bapi_timer_after(manager, 1.0f, count_fire, (void *)&g_index_values[0]) > 0);
	CHECK(bapi_timer_every(manager, 1.0f, count_fire, (void *)&g_index_values[1]) > 0);
	CHECK(bapi_timer_count(manager) == 2);

	bapi_timer_manager_destroy(manager);
	CHECK(g_fires[0] == 0);
	CHECK(g_fires[1] == 0);
	return 0;
}

int main(void)
{
	if (test_one_shot() != 0) return 1;
	if (test_repeating_and_cancel() != 0) return 1;
	if (test_algorithm_details() != 0) return 1;
	if (test_multiple_timers_and_ids() != 0) return 1;
	if (test_cancel_and_dt_edge_cases() != 0) return 1;
	if (test_cancel_from_callback() != 0) return 1;
	if (test_timer_added_from_callback() != 0) return 1;
	if (test_destroy_does_not_fire() != 0) return 1;

	printf("timer_test: ok\n");
	return 0;
}
