#include "BridgeEngine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ENTITY_MAX_NAME 63

typedef struct {
	float a;
	float b;
	float c;
	float d;
	float e;
	float f;
} entity_matrix_t;

struct bapi_entity_internal {
	char			  name[ENTITY_MAX_NAME + 1];
	void			 *user_data;
	bapi_entity_update_fn update;

	bapi_vec2_t position;
	float		rotation;
	bapi_vec2_t scale;

	int			 visible;
	bapi_texture_t texture;
	bapi_rect_t	 source;
	bapi_color_t tint;

	bapi_entity_t  parent;
	bapi_entity_t *children;
	int			   child_count;
	int			   child_capacity;
};

static entity_matrix_t entity_local_matrix(bapi_entity_t entity)
{
	float c = cosf(entity->rotation);
	float s = sinf(entity->rotation);

	entity_matrix_t matrix;
	matrix.a = c * entity->scale.x;
	matrix.b = s * entity->scale.x;
	matrix.c = -s * entity->scale.y;
	matrix.d = c * entity->scale.y;
	matrix.e = entity->position.x;
	matrix.f = entity->position.y;
	return matrix;
}

static entity_matrix_t entity_matrix_multiply(const entity_matrix_t *outer,
											  const entity_matrix_t *inner)
{
	entity_matrix_t result;
	result.a = outer->a * inner->a + outer->c * inner->b;
	result.b = outer->b * inner->a + outer->d * inner->b;
	result.c = outer->a * inner->c + outer->c * inner->d;
	result.d = outer->b * inner->c + outer->d * inner->d;
	result.e = outer->a * inner->e + outer->c * inner->f + outer->e;
	result.f = outer->b * inner->e + outer->d * inner->f + outer->f;
	return result;
}

static entity_matrix_t entity_world_matrix(bapi_entity_t entity)
{
	entity_matrix_t local = entity_local_matrix(entity);
	if (entity->parent == NULL) return local;
	entity_matrix_t parent = entity_world_matrix(entity->parent);
	return entity_matrix_multiply(&parent, &local);
}

static int entity_is_ancestor(bapi_entity_t ancestor, bapi_entity_t entity)
{
	for (bapi_entity_t current = entity; current != NULL; current = current->parent) {
		if (current == ancestor) return 1;
	}
	return 0;
}

static void entity_detach_from_parent(bapi_entity_t entity)
{
	if (entity->parent != NULL) bapi_entity_remove_child(entity->parent, entity);
}

bapi_entity_t bapi_entity_create(const char *name)
{
	bapi_entity_t entity = calloc(1, sizeof(*entity));
	if (entity == NULL) return NULL;

	if (name != NULL && name[0] != '\0') {
		strncpy(entity->name, name, ENTITY_MAX_NAME);
		entity->name[ENTITY_MAX_NAME] = '\0';
	} else {
		strcpy(entity->name, "entity");
	}
	entity->scale	= (bapi_vec2_t){1.0f, 1.0f};
	entity->visible = 1;
	entity->tint	= (bapi_color_t){255, 255, 255, 255};
	return entity;
}

void bapi_entity_destroy(bapi_entity_t entity)
{
	if (entity == NULL) return;
	entity_detach_from_parent(entity);
	while (entity->child_count > 0) {
		bapi_entity_destroy(entity->children[entity->child_count - 1]);
	}
	free(entity->children);
	free(entity);
}

const char *bapi_entity_get_name(bapi_entity_t entity)
{
	return entity != NULL ? entity->name : NULL;
}

void *bapi_entity_get_user_data(bapi_entity_t entity)
{
	return entity != NULL ? entity->user_data : NULL;
}

void bapi_entity_set_user_data(bapi_entity_t entity, void *user_data)
{
	if (entity != NULL) entity->user_data = user_data;
}

void bapi_entity_set_position(bapi_entity_t entity, bapi_vec2_t position)
{
	if (entity != NULL) entity->position = position;
}

bapi_vec2_t bapi_entity_get_position(bapi_entity_t entity)
{
	return entity != NULL ? entity->position : (bapi_vec2_t){0.0f, 0.0f};
}

void bapi_entity_set_rotation(bapi_entity_t entity, float radians)
{
	if (entity != NULL) entity->rotation = radians;
}

float bapi_entity_get_rotation(bapi_entity_t entity)
{
	return entity != NULL ? entity->rotation : 0.0f;
}

void bapi_entity_set_scale(bapi_entity_t entity, bapi_vec2_t scale)
{
	if (entity != NULL) entity->scale = scale;
}

bapi_vec2_t bapi_entity_get_scale(bapi_entity_t entity)
{
	return entity != NULL ? entity->scale : (bapi_vec2_t){1.0f, 1.0f};
}

bapi_vec2_t bapi_entity_get_world_position(bapi_entity_t entity)
{
	if (entity == NULL) return (bapi_vec2_t){0.0f, 0.0f};
	entity_matrix_t matrix = entity_world_matrix(entity);
	return (bapi_vec2_t){matrix.e, matrix.f};
}

float bapi_entity_get_world_rotation(bapi_entity_t entity)
{
	float rotation = 0.0f;
	for (bapi_entity_t current = entity; current != NULL; current = current->parent) {
		rotation += current->rotation;
	}
	return rotation;
}

bapi_vec2_t bapi_entity_get_world_scale(bapi_entity_t entity)
{
	bapi_vec2_t scale = {1.0f, 1.0f};
	for (bapi_entity_t current = entity; current != NULL; current = current->parent) {
		scale.x *= current->scale.x;
		scale.y *= current->scale.y;
	}
	return scale;
}

int bapi_entity_add_child(bapi_entity_t parent, bapi_entity_t child)
{
	if (parent == NULL || child == NULL || parent == child) return -1;
	if (child->parent != NULL) return -1;
	if (entity_is_ancestor(child, parent)) return -1;

	if (parent->child_count >= parent->child_capacity) {
		int			  new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
		bapi_entity_t *grown = realloc(parent->children, (size_t)new_capacity * sizeof(*grown));
		if (grown == NULL) return -1;
		parent->children	   = grown;
		parent->child_capacity = new_capacity;
	}
	parent->children[parent->child_count++] = child;
	child->parent							= parent;
	return 0;
}

int bapi_entity_remove_child(bapi_entity_t parent, bapi_entity_t child)
{
	if (parent == NULL || child == NULL || child->parent != parent) return -1;
	for (int i = 0; i < parent->child_count; i++) {
		if (parent->children[i] == child) {
			for (int j = i; j + 1 < parent->child_count; j++) {
				parent->children[j] = parent->children[j + 1];
			}
			parent->child_count--;
			child->parent = NULL;
			return 0;
		}
	}
	return -1;
}

bapi_entity_t bapi_entity_get_parent(bapi_entity_t entity)
{
	return entity != NULL ? entity->parent : NULL;
}

int bapi_entity_get_child_count(bapi_entity_t entity)
{
	return entity != NULL ? entity->child_count : 0;
}

bapi_entity_t bapi_entity_get_child(bapi_entity_t entity, int index)
{
	if (entity == NULL || index < 0 || index >= entity->child_count) return NULL;
	return entity->children[index];
}

bapi_entity_t bapi_entity_find_child(bapi_entity_t entity, const char *name)
{
	if (entity == NULL || name == NULL) return NULL;
	for (int i = 0; i < entity->child_count; i++) {
		if (strcmp(entity->children[i]->name, name) == 0) return entity->children[i];
	}
	return NULL;
}

void bapi_entity_set_visible(bapi_entity_t entity, int visible)
{
	if (entity != NULL) entity->visible = visible != 0;
}

int bapi_entity_is_visible(bapi_entity_t entity)
{
	return entity != NULL ? entity->visible : 0;
}

void bapi_entity_set_texture(bapi_entity_t entity, bapi_texture_t texture, bapi_rect_t source)
{
	if (entity == NULL) return;
	entity->texture = texture;
	entity->source	= source;
}

bapi_texture_t bapi_entity_get_texture(bapi_entity_t entity)
{
	return entity ? entity->texture : NULL;
}

bapi_rect_t bapi_entity_get_source(bapi_entity_t entity)
{
	return entity ? entity->source : (bapi_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

bapi_color_t bapi_entity_get_tint(bapi_entity_t entity)
{
	return entity ? entity->tint : (bapi_color_t){255, 255, 255, 255};
}

void bapi_entity_set_tint(bapi_entity_t entity, bapi_color_t tint)
{
	if (entity != NULL) entity->tint = tint;
}

void bapi_entity_set_update(bapi_entity_t entity, bapi_entity_update_fn update)
{
	if (entity != NULL) entity->update = update;
}

void bapi_entity_update(bapi_entity_t entity, float delta_time)
{
	if (entity == NULL) return;
	if (entity->update != NULL) entity->update(entity, delta_time);
	for (int i = 0; i < entity->child_count; i++) {
		bapi_entity_update(entity->children[i], delta_time);
	}
}

void bapi_entity_draw(bapi_entity_t entity)
{
	if (entity == NULL || !entity->visible) return;

	bapi_push_transform(entity->position, entity->rotation, entity->scale);

	if (entity->texture != NULL) {
		int width = 0, height = 0;
		bapi_texture_get_size(entity->texture, &width, &height);
		float source_width  = entity->source.w > 0.0f ? entity->source.w : (float)width;
		float source_height = entity->source.h > 0.0f ? entity->source.h : (float)height;

		bapi_texture_draw_t params =
			bapi_texture_draw_default((bapi_rect_t){0.0f, 0.0f, source_width, source_height});
		params.src	  = entity->source;
		params.pivot_x = 0.5f;
		params.pivot_y = 0.5f;
		params.tint	  = entity->tint;
		bapi_texture_draw(entity->texture, &params);
	}

	for (int i = 0; i < entity->child_count; i++) {
		bapi_entity_draw(entity->children[i]);
	}
	bapi_pop_transform();
}
