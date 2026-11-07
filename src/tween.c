#include "BridgeEngine.h"

#include <stdlib.h>

/* Manager-owned float tweens; out-back overshoots by design. */

struct bapi_tween_internal {
	float						from;
	float						to;
	float						duration;
	bapi_ease_t					ease;
	int							loop;
	float						elapsed;
	int							finished;
	struct bapi_tween_internal *next;
};

struct bapi_tween_manager_internal {
	bapi_tween_t head;
};

static float clamp01(float t)
{
	if (t < 0.0f) return 0.0f;
	if (t > 1.0f) return 1.0f;
	return t;
}

float bapi_ease(bapi_ease_t ease, float t)
{
	t = clamp01(t);

	switch (ease) {
	case BAPI_EASE_LINEAR:
		return t;
	case BAPI_EASE_IN_QUAD:
		return t * t;
	case BAPI_EASE_OUT_QUAD: {
		float u = 1.0f - t;
		return 1.0f - u * u;
	}
	case BAPI_EASE_IN_OUT_QUAD: {
		float u;
		if (t < 0.5f) return 2.0f * t * t;
		u = -2.0f * t + 2.0f;
		return 1.0f - u * u * 0.5f;
	}
	case BAPI_EASE_IN_CUBIC:
		return t * t * t;
	case BAPI_EASE_OUT_CUBIC: {
		float u = 1.0f - t;
		return 1.0f - u * u * u;
	}
	case BAPI_EASE_IN_OUT_CUBIC: {
		float u;
		if (t < 0.5f) return 4.0f * t * t * t;
		u = -2.0f * t + 2.0f;
		return 1.0f - u * u * u * 0.5f;
	}
	case BAPI_EASE_OUT_BACK: {
		/* Standard easeOutBack constants: c1 = 1.70158, c3 = c1 + 1. */
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		float		u  = t - 1.0f;
		return 1.0f + c3 * u * u * u + c1 * u * u;
	}
	default:
		/* Unknown easing values behave like linear. */
		return t;
	}
}

bapi_tween_manager_t bapi_tween_manager_create(void)
{
	bapi_tween_manager_t manager = (bapi_tween_manager_t)malloc(sizeof(*manager));
	if (manager == NULL) return NULL;

	manager->head = NULL;
	return manager;
}

void bapi_tween_manager_destroy(bapi_tween_manager_t manager)
{
	bapi_tween_t tween;

	if (manager == NULL) return;

	tween = manager->head;
	while (tween != NULL) {
		bapi_tween_t next = tween->next;
		free(tween);
		tween = next;
	}
	free(manager);
}

bapi_tween_t bapi_tween_create(bapi_tween_manager_t manager, float from, float to, float duration,
							   bapi_ease_t ease, int loop)
{
	bapi_tween_t tween;

	if (manager == NULL || duration <= 0.0f) return NULL;

	tween = (bapi_tween_t)malloc(sizeof(*tween));
	if (tween == NULL) return NULL;

	tween->from		= from;
	tween->to		= to;
	tween->duration = duration;
	tween->ease		= ease;
	tween->loop		= loop ? 1 : 0;
	tween->elapsed	= 0.0f;
	tween->finished = 0;
	tween->next		= manager->head;
	manager->head	= tween;

	return tween;
}

void bapi_tween_destroy(bapi_tween_manager_t manager, bapi_tween_t tween)
{
	bapi_tween_t prev;
	bapi_tween_t cur;

	if (tween == NULL) return;
	if (manager == NULL) {
		free(tween);
		return;
	}

	prev = NULL;
	cur	 = manager->head;
	while (cur != NULL && cur != tween) {
		prev = cur;
		cur	 = cur->next;
	}
	if (cur == NULL) return; /* Not owned by this manager; leave it alone. */
	if (prev == NULL)
		manager->head = cur->next;
	else
		prev->next = cur->next;

	free(tween);
}

void bapi_tween_manager_update(bapi_tween_manager_t manager, float delta_time)
{
	bapi_tween_t tween;

	if (manager == NULL) return;
	if (delta_time < 0.0f) delta_time = 0.0f;

	for (tween = manager->head; tween != NULL; tween = tween->next) {
		if (!tween->loop && tween->finished) continue;

		tween->elapsed += delta_time;

		if (tween->loop) {
			while (tween->elapsed >= tween->duration) tween->elapsed -= tween->duration;
		} else if (tween->elapsed >= tween->duration) {
			tween->elapsed	= tween->duration;
			tween->finished = 1;
		}
	}
}

float bapi_tween_value(bapi_tween_t tween)
{
	float t;

	if (tween == NULL) return 0.0f;
	if (tween->duration <= 0.0f) return tween->to;

	t = clamp01(tween->elapsed / tween->duration);
	return tween->from + (tween->to - tween->from) * bapi_ease(tween->ease, t);
}

int bapi_tween_is_finished(bapi_tween_t tween)
{
	if (tween == NULL) return 1;
	return tween->finished ? 1 : 0;
}
