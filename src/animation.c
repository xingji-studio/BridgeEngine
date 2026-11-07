/* Clip and animator state. Clips copy their arrays and borrow the texture;
 * duplicate clip names replace the old clip. Transitions are checked before
 * frame advancement, in insertion order.
 */
#include "BridgeEngine.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	char		  *name;
	bapi_texture_t texture;
	bapi_rect_t	  *frames;
	float		  *durations;
	int			   frame_count;
	int			   loop;
	int			   current_frame;
	float		   elapsed;
} bapi_anim_clip_t;

typedef struct {
	char *name;
	int	  value;
} bapi_anim_param_t;

typedef struct {
	char *from;
	char *to;
	char *parameter;
	int	  value;
} bapi_anim_transition_t;

struct bapi_animator_internal {
	bapi_anim_clip_t	   *clips;
	int						clip_count;
	int						clip_capacity;
	bapi_anim_param_t	   *params;
	int						param_count;
	int						param_capacity;
	bapi_anim_transition_t *transitions;
	int						transition_count;
	int						transition_capacity;
	int						active_clip; /* index into clips, -1 when none */
	int						finished;
};

static char *anim_strdup(const char *src)
{
	size_t len;
	char  *copy;

	if (!src) return NULL;
	len	 = strlen(src) + 1;
	copy = (char *)malloc(len);
	if (!copy) return NULL;
	memcpy(copy, src, len);
	return copy;
}

static int anim_grow(void **array, int *capacity, int needed, size_t item_size)
{
	int	  new_capacity;
	void *grown;

	if (needed <= *capacity) return 0;
	new_capacity = *capacity > 0 ? *capacity * 2 : 8;
	while (new_capacity < needed) new_capacity *= 2;
	grown = realloc(*array, (size_t)new_capacity * item_size);
	if (!grown) return -1;
	*array	  = grown;
	*capacity = new_capacity;
	return 0;
}

static int anim_find_clip(bapi_animator_t animator, const char *name)
{
	int i;

	if (!animator || !name) return -1;
	for (i = 0; i < animator->clip_count; i++) {
		if (strcmp(animator->clips[i].name, name) == 0) return i;
	}
	return -1;
}

static int anim_find_param(bapi_animator_t animator, const char *name)
{
	int i;

	if (!animator || !name) return -1;
	for (i = 0; i < animator->param_count; i++) {
		if (strcmp(animator->params[i].name, name) == 0) return i;
	}
	return -1;
}

static int anim_param_value(bapi_animator_t animator, const char *name)
{
	int index = anim_find_param(animator, name);

	return index >= 0 ? animator->params[index].value : 0;
}

static void anim_free_clip(bapi_anim_clip_t *clip)
{
	if (!clip) return;
	free(clip->name);
	free(clip->frames);
	free(clip->durations);
	memset(clip, 0, sizeof(*clip));
}

bapi_animator_t bapi_animator_create(void)
{
	bapi_animator_t animator = (bapi_animator_t)calloc(1, sizeof(*animator));

	if (!animator) return NULL;
	animator->active_clip = -1;
	animator->finished	  = 1;
	return animator;
}

void bapi_animator_destroy(bapi_animator_t animator)
{
	int i;

	if (!animator) return;
	for (i = 0; i < animator->clip_count; i++) anim_free_clip(&animator->clips[i]);
	for (i = 0; i < animator->param_count; i++) free(animator->params[i].name);
	for (i = 0; i < animator->transition_count; i++) {
		free(animator->transitions[i].from);
		free(animator->transitions[i].to);
		free(animator->transitions[i].parameter);
	}
	free(animator->clips);
	free(animator->params);
	free(animator->transitions);
	free(animator);
}

int bapi_animator_add_clip(bapi_animator_t animator, const char *name, bapi_texture_t texture,
						   const bapi_rect_t *frames, const float *durations, int frame_count,
						   int loop)
{
	bapi_anim_clip_t *clip;
	char			 *name_copy;
	bapi_rect_t		 *frame_copy;
	float			 *duration_copy;
	int				  index;
	int				  i;

	if (!animator || !name || name[0] == '\0') return -1;
	if (!frames || !durations || frame_count <= 0) return -1;
	for (i = 0; i < frame_count; i++) {
		if (!(durations[i] >= 0.0f)) return -1;
	}

	name_copy = anim_strdup(name);
	if (!name_copy) return -1;
	frame_copy	  = (bapi_rect_t *)malloc((size_t)frame_count * sizeof(*frame_copy));
	duration_copy = (float *)malloc((size_t)frame_count * sizeof(*duration_copy));
	if (!frame_copy || !duration_copy) {
		free(name_copy);
		free(frame_copy);
		free(duration_copy);
		return -1;
	}
	memcpy(frame_copy, frames, (size_t)frame_count * sizeof(*frame_copy));
	memcpy(duration_copy, durations, (size_t)frame_count * sizeof(*duration_copy));

	index = anim_find_clip(animator, name);
	if (index < 0) {
		if (anim_grow((void **)&animator->clips, &animator->clip_capacity, animator->clip_count + 1,
					  sizeof(*animator->clips)) != 0) {
			free(name_copy);
			free(frame_copy);
			free(duration_copy);
			return -1;
		}
		index = animator->clip_count++;
		memset(&animator->clips[index], 0, sizeof(animator->clips[index]));
	} else {
		/* Duplicate name: replace the existing clip, keeping heap buffers safe. */
		anim_free_clip(&animator->clips[index]);
	}

	clip				= &animator->clips[index];
	clip->name			= name_copy;
	clip->texture		= texture;
	clip->frames		= frame_copy;
	clip->durations		= duration_copy;
	clip->frame_count	= frame_count;
	clip->loop			= loop ? 1 : 0;
	clip->current_frame = 0;
	clip->elapsed		= 0.0f;
	if (animator->active_clip == index) animator->finished = 0;
	return 0;
}

int bapi_animator_play(bapi_animator_t animator, const char *name)
{
	int index;

	if (!animator || !name) return -1;
	index = anim_find_clip(animator, name);
	if (index < 0) return -1;
	animator->active_clip				 = index;
	animator->clips[index].current_frame = 0;
	animator->clips[index].elapsed		 = 0.0f;
	animator->finished					 = 0;
	return 0;
}

int bapi_animator_add_transition(bapi_animator_t animator, const char *from, const char *to,
								 const char *parameter, int value)
{
	bapi_anim_transition_t *transition;
	char				   *from_copy;
	char				   *to_copy;
	char				   *parameter_copy;
	int						index;

	if (!animator || !from || !to || !parameter) return -1;
	if (from[0] == '\0' || to[0] == '\0' || parameter[0] == '\0') return -1;

	from_copy	   = anim_strdup(from);
	to_copy		   = anim_strdup(to);
	parameter_copy = anim_strdup(parameter);
	if (!from_copy || !to_copy || !parameter_copy) {
		free(from_copy);
		free(to_copy);
		free(parameter_copy);
		return -1;
	}
	if (anim_grow((void **)&animator->transitions, &animator->transition_capacity,
				  animator->transition_count + 1, sizeof(*animator->transitions)) != 0) {
		free(from_copy);
		free(to_copy);
		free(parameter_copy);
		return -1;
	}
	index				  = animator->transition_count++;
	transition			  = &animator->transitions[index];
	transition->from	  = from_copy;
	transition->to		  = to_copy;
	transition->parameter = parameter_copy;
	transition->value	  = value ? 1 : 0;
	return 0;
}

void bapi_animator_set_bool(bapi_animator_t animator, const char *parameter, int value)
{
	int index;
	int normalized;

	if (!animator || !parameter || parameter[0] == '\0') return;
	normalized = value ? 1 : 0;
	index	   = anim_find_param(animator, parameter);
	if (index >= 0) {
		animator->params[index].value = normalized;
		return;
	}
	if (anim_grow((void **)&animator->params, &animator->param_capacity, animator->param_count + 1,
				  sizeof(*animator->params)) != 0)
		return;
	index						 = animator->param_count++;
	animator->params[index].name = anim_strdup(parameter);
	if (!animator->params[index].name) {
		animator->param_count--;
		return;
	}
	animator->params[index].value = normalized;
}

static void anim_advance(bapi_animator_t animator, bapi_anim_clip_t *clip, float delta_time)
{
	float remaining = delta_time;
	int	  guard		= 0;

	while (remaining > 0.0f && guard++ < 1000000) {
		float duration = clip->durations[clip->current_frame];
		float need;

		if (duration < 1e-6f) duration = 1e-6f; /* degenerate durations stay finite */
		need = duration - clip->elapsed;
		if (remaining < need) {
			clip->elapsed += remaining;
			return;
		}
		remaining -= need;
		clip->elapsed = 0.0f;
		if (clip->current_frame + 1 < clip->frame_count) {
			clip->current_frame++;
		} else if (clip->loop) {
			clip->current_frame = 0;
		} else {
			clip->current_frame = clip->frame_count - 1;
			clip->elapsed		= duration;
			animator->finished	= 1;
			return;
		}
	}
}

void bapi_animator_update(bapi_animator_t animator, float delta_time)
{
	bapi_anim_clip_t *clip;
	int				  i;

	if (!animator || animator->active_clip < 0) return;
	clip = &animator->clips[animator->active_clip];

	/* Evaluate transitions first: the first matching one wins and consumes no time. */
	for (i = 0; i < animator->transition_count; i++) {
		bapi_anim_transition_t *transition = &animator->transitions[i];

		if (strcmp(transition->from, clip->name) != 0) continue;
		if (anim_param_value(animator, transition->parameter) != transition->value) continue;
		if (bapi_animator_play(animator, transition->to) == 0) return;
		/* A transition to an unknown clip falls through to the next candidate. */
	}

	if (animator->finished) return;
	if (!(delta_time > 0.0f)) return;
	anim_advance(animator, clip, delta_time);
}

bapi_rect_t bapi_animator_get_frame(bapi_animator_t animator)
{
	bapi_anim_clip_t *clip;

	if (!animator || animator->active_clip < 0) return (bapi_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	clip = &animator->clips[animator->active_clip];
	return clip->frames[clip->current_frame];
}

bapi_texture_t bapi_animator_get_texture(bapi_animator_t animator)
{
	if (!animator || animator->active_clip < 0) return NULL;
	return animator->clips[animator->active_clip].texture;
}

const char *bapi_animator_get_clip(bapi_animator_t animator)
{
	if (!animator || animator->active_clip < 0) return "";
	return animator->clips[animator->active_clip].name;
}

int bapi_animator_is_finished(bapi_animator_t animator)
{
	if (!animator || animator->active_clip < 0) return 1;
	return animator->finished ? 1 : 0;
}
