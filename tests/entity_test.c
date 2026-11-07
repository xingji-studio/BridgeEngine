#include "BridgeEngine.h"
#include "internal/engine/texture_internal.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct plat_window {
	int unused;
};

struct plat_renderer {
	int unused;
};

static struct plat_renderer fake_renderer;
static int					dummy_texture;
static int					draw_calls;
static float				rec_src_w, rec_src_h;
static float				rec_dst_x, rec_dst_y, rec_dst_w, rec_dst_h;

plat_renderer_t bapi_internal_get_renderer(void)
{
	return &fake_renderer;
}

uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size)
{
	(void)path;
	(void)out_size;
	return NULL;
}

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

static void render_texture_ex(plat_renderer_t renderer, plat_texture_t texture,
							  const plat_rectf_t *src, const plat_rectf_t *dst,
							  const plat_pointf_t *pivot, float rotation, int flip, uint8_t r,
							  uint8_t g, uint8_t b, uint8_t a)
{
	(void)renderer;
	(void)texture;
	(void)pivot;
	(void)rotation;
	(void)flip;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
	draw_calls++;
	if (src) {
		rec_src_w = src->w;
		rec_src_h = src->h;
	}
	rec_dst_x = dst->x;
	rec_dst_y = dst->y;
	rec_dst_w = dst->w;
	rec_dst_h = dst->h;
}

static void render_texture(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
						   float w, float h)
{
	(void)renderer;
	(void)texture;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
}

static const plat_interface_t platform = {
	.renderer = {.render_texture = render_texture, .render_texture_ex = render_texture_ex},
};

static const plat_interface_t *current_platform;

const plat_interface_t *plat_get(void)
{
	return current_platform;
}

static int update_count;
static void on_update(bapi_entity_t entity, float dt)
{
	(void)entity;
	(void)dt;
	update_count++;
}

int main(void)
{
	int result = 1;
	current_platform = &platform;

	struct bapi_texture_internal texture;
	memset(&texture, 0, sizeof(texture));
	texture.platform_texture = (plat_texture_t)&dummy_texture;
	texture.width			 = 16;
	texture.height			 = 16;

	bapi_entity_t parent = bapi_entity_create("parent");
	bapi_entity_t child	 = bapi_entity_create("child");
	result &= expect(parent != NULL && child != NULL, "entities are created");
	result &= expect(strcmp(bapi_entity_get_name(parent), "parent") == 0, "entity name");
	result &= expect(bapi_entity_add_child(parent, child) == 0, "child is attached");
	result &= expect(bapi_entity_get_child_count(parent) == 1, "child count");
	result &= expect(bapi_entity_get_child(parent, 0) == child, "child lookup by index");
	result &= expect(bapi_entity_find_child(parent, "child") == child, "child lookup by name");
	result &= expect(bapi_entity_add_child(parent, child) != 0, "double attach is rejected");
	result &= expect(bapi_entity_add_child(child, parent) != 0, "cycle is rejected");

	bapi_entity_set_position(parent, (bapi_vec2_t){10.0f, 20.0f});
	bapi_entity_set_scale(parent, (bapi_vec2_t){2.0f, 2.0f});
	bapi_entity_set_position(child, (bapi_vec2_t){5.0f, 5.0f});
	bapi_vec2_t world = bapi_entity_get_world_position(child);
	result &= expect(fabsf(world.x - 20.0f) < 0.001f && fabsf(world.y - 30.0f) < 0.001f,
					 "parent scale and translation compose");

	bapi_entity_set_rotation(parent, 1.57079632679f);
	bapi_entity_set_position(parent, (bapi_vec2_t){0.0f, 0.0f});
	bapi_entity_set_scale(parent, (bapi_vec2_t){1.0f, 1.0f});
	bapi_entity_set_position(child, (bapi_vec2_t){1.0f, 0.0f});
	world = bapi_entity_get_world_position(child);
	result &= expect(fabsf(world.x) < 0.001f && fabsf(world.y - 1.0f) < 0.001f,
					 "parent rotation composes");
	result &= expect(fabsf(bapi_entity_get_world_rotation(child) - 1.5707963f) < 0.001f,
					 "world rotation accumulates");
	bapi_entity_set_scale(parent, (bapi_vec2_t){2.0f, 3.0f});
	bapi_entity_set_scale(child, (bapi_vec2_t){0.5f, 0.5f});
	bapi_vec2_t scale = bapi_entity_get_world_scale(child);
	result &= expect(fabsf(scale.x - 1.0f) < 0.001f && fabsf(scale.y - 1.5f) < 0.001f,
					 "world scale multiplies");

	bapi_entity_set_rotation(parent, 0.0f);
	bapi_entity_set_scale(parent, (bapi_vec2_t){1.0f, 1.0f});
	bapi_entity_set_position(parent, (bapi_vec2_t){10.0f, 20.0f});
	bapi_entity_set_position(child, (bapi_vec2_t){0.0f, 0.0f});
	bapi_entity_set_texture(child, &texture, (bapi_rect_t){0.0f, 0.0f, 16.0f, 16.0f});
	draw_calls = 0;
	bapi_entity_draw(parent);
	result &= expect(draw_calls == 1, "entity draw reaches the texture backend");
	result &= expect(rec_src_w == 16.0f && rec_dst_x == 10.0f && rec_dst_y == 20.0f,
					 "sprite is centered on the parent-space origin");

	draw_calls = 0;
	bapi_entity_set_visible(parent, 0);
	bapi_entity_draw(parent);
	result &= expect(draw_calls == 0, "invisible parent hides its subtree");
	bapi_entity_set_visible(parent, 1);

	update_count = 0;
	bapi_entity_set_update(child, on_update);
	bapi_entity_update(parent, 0.016f);
	result &= expect(update_count == 1, "entity update recurses");

	result &= expect(bapi_entity_remove_child(parent, child) == 0, "child detach");
	result &= expect(bapi_entity_get_parent(child) == NULL, "detached child has no parent");
	result &= expect(bapi_entity_get_child_count(parent) == 0, "child count after detach");
	bapi_entity_destroy(child);

	bapi_entity_t second = bapi_entity_create("second");
	result &= expect(bapi_entity_add_child(parent, second) == 0, "re-attach after detach");
	bapi_entity_destroy(parent);

	return result ? 0 : 1;
}
