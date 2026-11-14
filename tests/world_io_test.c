#include "BridgeEngine.h"
#include "internal/bapi_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- mock texture layer -------------------------------------------------- */

struct bapi_texture_internal {
	const char *path;
};

#define MOCK_TEXTURE_MAX 8

static struct bapi_texture_internal g_mock_textures[MOCK_TEXTURE_MAX];
static char							g_mock_paths[MOCK_TEXTURE_MAX][256];
static int							g_mock_texture_count;

bapi_texture_t bapi_texture_load(const char *filepath)
{
	if (filepath == NULL || filepath[0] == '\0') return NULL;
	if (strcmp(filepath, "missing.png") == 0) return NULL;
	if (g_mock_texture_count >= MOCK_TEXTURE_MAX) return NULL;

	int index = g_mock_texture_count++;
	snprintf(g_mock_paths[index], sizeof(g_mock_paths[index]), "%s", filepath);
	g_mock_textures[index].path = g_mock_paths[index];
	return &g_mock_textures[index];
}

const char *bapi_texture_get_path(bapi_texture_t texture)
{
	return texture != NULL ? texture->path : NULL;
}

void bapi_texture_get_size(bapi_texture_t texture, int *w, int *h)
{
	(void)texture;
	if (w) *w = 16;
	if (h) *h = 16;
}

bapi_texture_draw_t bapi_texture_draw_default(bapi_rect_t dst)
{
	bapi_texture_draw_t params;
	memset(&params, 0, sizeof(params));
	params.dst	   = dst;
	params.pivot_x = 0.5f;
	params.pivot_y = 0.5f;
	params.tint	   = (bapi_color_t){255, 255, 255, 255};
	return params;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	(void)texture;
	(void)params;
}

/* --- helpers ------------------------------------------------------------- */

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static int nearly_equal(float a, float b)
{
	return fabsf(a - b) < 0.001f;
}

static char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) return NULL;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long size = ftell(file);
	if (size < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	char *buffer = malloc((size_t)size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
		free(buffer);
		fclose(file);
		return NULL;
	}
	buffer[size] = '\0';
	fclose(file);
	return buffer;
}

static void check_tint(bapi_entity_t entity, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int *ok)
{
	bapi_color_t tint = bapi_entity_get_tint(entity);
	if (tint.r != r || tint.g != g || tint.b != b || tint.a != a) *ok = 0;
}

static void check_transform(bapi_entity_t entity, float x, float y, float rotation, float sx,
							float sy, int *ok)
{
	bapi_vec2_t position = bapi_entity_get_position(entity);
	bapi_vec2_t scale	 = bapi_entity_get_scale(entity);
	if (!nearly_equal(position.x, x) || !nearly_equal(position.y, y)) *ok = 0;
	if (!nearly_equal(bapi_entity_get_rotation(entity), rotation)) *ok = 0;
	if (!nearly_equal(scale.x, sx) || !nearly_equal(scale.y, sy)) *ok = 0;
}

/* --- tests --------------------------------------------------------------- */

static int test_roundtrip_file(void)
{
	const char *path	  = "world_io_test_roundtrip.bworld";
	const char *path_copy = "world_io_test_roundtrip_2.bworld";
	remove(path);
	remove(path_copy);
	g_mock_texture_count = 0;

	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);

	bapi_entity_t root = bapi_world_spawn(world, "root");
	CHECK(root != NULL);
	bapi_entity_set_position(root, (bapi_vec2_t){1.5f, -2.25f});
	bapi_entity_set_rotation(root, 0.5f);
	bapi_entity_set_scale(root, (bapi_vec2_t){2.0f, 3.0f});
	bapi_entity_set_visible(root, 1);
	bapi_entity_set_tint(root, (bapi_color_t){10, 20, 30, 40});

	bapi_entity_t child_a = bapi_entity_create("child_a");
	bapi_entity_t child_b = bapi_entity_create("child_b");
	CHECK(child_a != NULL && child_b != NULL);
	CHECK(bapi_entity_add_child(root, child_a) == 0);
	CHECK(bapi_entity_add_child(root, child_b) == 0);
	bapi_entity_set_position(child_a, (bapi_vec2_t){4.0f, 5.0f});
	bapi_entity_set_rotation(child_a, -1.0f);
	bapi_entity_set_scale(child_a, (bapi_vec2_t){0.5f, 0.25f});
	bapi_entity_set_visible(child_a, 0);
	bapi_entity_set_tint(child_a, (bapi_color_t){1, 2, 3, 4});

	bapi_texture_t texture = bapi_texture_load("assets/hero.png");
	CHECK(texture != NULL);
	bapi_entity_set_texture(child_a, texture, (bapi_rect_t){2.0f, 3.0f, 16.0f, 24.0f});

	bapi_entity_t grandchild = bapi_entity_create("grandchild");
	CHECK(grandchild != NULL);
	CHECK(bapi_entity_add_child(child_a, grandchild) == 0);
	bapi_entity_set_position(grandchild, (bapi_vec2_t){-3.0f, -4.0f});
	bapi_entity_set_rotation(grandchild, 2.0f);
	bapi_entity_set_scale(grandchild, (bapi_vec2_t){1.5f, 1.5f});
	bapi_entity_set_visible(grandchild, 1);
	bapi_entity_set_tint(grandchild, (bapi_color_t){255, 128, 0, 255});

	bapi_entity_set_position(child_b, (bapi_vec2_t){7.0f, 8.0f});
	bapi_entity_set_tint(child_b, (bapi_color_t){200, 201, 202, 203});

	/* The .bworld format stores x/y/zoom only; viewport/rotation stay default. */
	bapi_camera_t camera = {12.5f, -3.25f, 1.75f, 0.0f, 0.0f, 0.0f};
	bapi_world_set_camera(world, camera);

	CHECK(bapi_world_save(world, path) == 0);
	CHECK(bapi_get_last_error()[0] == '\0');

	char *content = read_file(path);
	CHECK(content != NULL);
	CHECK(strncmp(content, "BWORLD 1\n", 9) == 0);
	CHECK(strstr(content, "camera 12.5 -3.25 1.75\n") != NULL);
	CHECK(strstr(content, "entity root\npos 1.5 -2.25\nrot 0.5\nscale 2 3\nvisible 1\n") != NULL);
	CHECK(strstr(content, "sprite assets/hero.png 2 3 16 24\n") != NULL);
	CHECK(strstr(content, "tint 255 128 0 255\n") != NULL);
	CHECK(strstr(content, "entity grandchild\n") != NULL);

	/* Successful save leaves no temp file behind. */
	FILE *temp = fopen("world_io_test_roundtrip.bworld.tmp", "rb");
	CHECK(temp == NULL);

	bapi_world_t loaded = bapi_world_create();
	CHECK(loaded != NULL);
	CHECK(bapi_world_load(loaded, path) == 0);
	CHECK(bapi_world_entity_count(loaded) == 1);

	bapi_camera_t readback;
	bapi_world_get_camera(loaded, &readback);
	CHECK(nearly_equal(readback.x, camera.x));
	CHECK(nearly_equal(readback.y, camera.y));
	CHECK(nearly_equal(readback.zoom, camera.zoom));
	CHECK(nearly_equal(readback.rotation, camera.rotation));
	CHECK(nearly_equal(readback.viewport_w, camera.viewport_w));
	CHECK(nearly_equal(readback.viewport_h, camera.viewport_h));

	int			  ok		  = 1;
	bapi_entity_t loaded_root = bapi_world_get_entity(loaded, 0);
	CHECK(loaded_root != NULL);
	CHECK(strcmp(bapi_entity_get_name(loaded_root), "root") == 0);
	CHECK(bapi_entity_get_parent(loaded_root) == NULL);
	check_transform(loaded_root, 1.5f, -2.25f, 0.5f, 2.0f, 3.0f, &ok);
	CHECK(bapi_entity_is_visible(loaded_root) == 1);
	check_tint(loaded_root, 10, 20, 30, 40, &ok);
	CHECK(bapi_entity_get_child_count(loaded_root) == 2);

	bapi_entity_t loaded_a = bapi_entity_get_child(loaded_root, 0);
	bapi_entity_t loaded_b = bapi_entity_get_child(loaded_root, 1);
	CHECK(loaded_a != NULL && loaded_b != NULL);
	CHECK(strcmp(bapi_entity_get_name(loaded_a), "child_a") == 0);
	CHECK(strcmp(bapi_entity_get_name(loaded_b), "child_b") == 0);
	CHECK(bapi_entity_get_parent(loaded_a) == loaded_root);
	CHECK(bapi_entity_get_parent(loaded_b) == loaded_root);
	check_transform(loaded_a, 4.0f, 5.0f, -1.0f, 0.5f, 0.25f, &ok);
	CHECK(bapi_entity_is_visible(loaded_a) == 0);
	check_tint(loaded_a, 1, 2, 3, 4, &ok);

	bapi_texture_t loaded_texture = bapi_entity_get_texture(loaded_a);
	CHECK(loaded_texture != NULL);
	CHECK(strcmp(bapi_texture_get_path(loaded_texture), "assets/hero.png") == 0);
	bapi_rect_t source = bapi_entity_get_source(loaded_a);
	CHECK(nearly_equal(source.x, 2.0f));
	CHECK(nearly_equal(source.y, 3.0f));
	CHECK(nearly_equal(source.w, 16.0f));
	CHECK(nearly_equal(source.h, 24.0f));

	CHECK(bapi_entity_get_child_count(loaded_a) == 1);
	bapi_entity_t loaded_grandchild = bapi_entity_get_child(loaded_a, 0);
	CHECK(loaded_grandchild != NULL);
	CHECK(strcmp(bapi_entity_get_name(loaded_grandchild), "grandchild") == 0);
	CHECK(bapi_entity_get_parent(loaded_grandchild) == loaded_a);
	check_transform(loaded_grandchild, -3.0f, -4.0f, 2.0f, 1.5f, 1.5f, &ok);
	check_tint(loaded_grandchild, 255, 128, 0, 255, &ok);

	check_transform(loaded_b, 7.0f, 8.0f, 0.0f, 1.0f, 1.0f, &ok);
	check_tint(loaded_b, 200, 201, 202, 203, &ok);
	CHECK(ok == 1);

	/* Re-saving the loaded world produces the same canonical text. */
	CHECK(bapi_world_save(loaded, path_copy) == 0);
	char *copy = read_file(path_copy);
	CHECK(copy != NULL);
	CHECK(strcmp(content, copy) == 0);

	free(copy);
	free(content);
	bapi_world_destroy(loaded);
	bapi_world_destroy(world);
	remove(path);
	remove(path_copy);
	return 0;
}

static int test_parser_tolerance(void)
{
	g_mock_texture_count = 0;

	/* Comments, blank lines, CRLF, unknown keys, nested camera line. */
	const char *text = "# leading comment\r\n"
					   "\r\n"
					   "BWORLD 1\r\n"
					   "unknown_top 1 2 3\r\n"
					   "camera 2.5 -3 1.5 # inline comment\r\n"
					   "entity root\r\n"
					   "  pos 1 2\r\n"
					   "  mystery_key 9 8 7\r\n"
					   "  rot 0.25\r\n"
					   "  scale 2 2\r\n"
					   "  visible 1\r\n"
					   "  tint 1 2 3 4\r\n"
					   "  camera 9 9 9\r\n"
					   "  entity child\r\n"
					   "    pos 3 4\r\n"
					   "    future_field whatever\r\n"
					   "  end\r\n"
					   "end\r\n";

	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	CHECK(bapi_world_load_from_memory(world, text, strlen(text)) == 0);
	CHECK(bapi_world_entity_count(world) == 1);

	bapi_camera_t camera;
	bapi_world_get_camera(world, &camera);
	CHECK(nearly_equal(camera.x, 2.5f));
	CHECK(nearly_equal(camera.y, -3.0f));
	CHECK(nearly_equal(camera.zoom, 1.5f));

	bapi_entity_t root = bapi_world_get_entity(world, 0);
	CHECK(root != NULL);
	CHECK(strcmp(bapi_entity_get_name(root), "root") == 0);
	int ok = 1;
	check_transform(root, 1.0f, 2.0f, 0.25f, 2.0f, 2.0f, &ok);
	check_tint(root, 1, 2, 3, 4, &ok);
	CHECK(bapi_entity_get_child_count(root) == 1);

	bapi_entity_t child = bapi_entity_get_child(root, 0);
	CHECK(child != NULL);
	CHECK(strcmp(bapi_entity_get_name(child), "child") == 0);
	bapi_vec2_t child_position = bapi_entity_get_position(child);
	CHECK(nearly_equal(child_position.x, 3.0f));
	CHECK(nearly_equal(child_position.y, 4.0f));
	CHECK(ok == 1);

	bapi_world_destroy(world);

	/* Missing camera line falls back to the documented default. */
	const char *no_camera = "BWORLD 1\nentity lonely\nend\n";
	world				  = bapi_world_create();
	CHECK(world != NULL);
	CHECK(bapi_world_load_from_memory(world, no_camera, strlen(no_camera)) == 0);
	bapi_world_get_camera(world, &camera);
	CHECK(nearly_equal(camera.x, 0.0f));
	CHECK(nearly_equal(camera.y, 0.0f));
	CHECK(nearly_equal(camera.zoom, 1.0f));
	bapi_world_destroy(world);
	return 0;
}

static int expect_load_failure(const char *text, const char *label)
{
	bapi_world_t world = bapi_world_create();
	if (world == NULL) {
		fprintf(stderr, "FAIL %s: world create\n", label);
		return 1;
	}
	bapi_set_last_error("");
	int result = bapi_world_load_from_memory(world, text, strlen(text));
	int failed =
		result != 0 && bapi_get_last_error()[0] != '\0' && bapi_world_entity_count(world) == 0;
	if (!failed) {
		fprintf(stderr, "FAIL %s: result=%d error='%s' count=%d\n", label, result,
				bapi_get_last_error(), bapi_world_entity_count(world));
	}
	bapi_world_destroy(world);
	return failed ? 0 : 1;
}

static int test_rejections(void)
{
	if (expect_load_failure("BWORLDX 1\n", "bad magic")) return 1;
	if (expect_load_failure("BWORLD 0\n", "version 0")) return 1;
	if (expect_load_failure("BWORLD 99\n", "version 99")) return 1;
	if (expect_load_failure("BWORLD\n", "missing version")) return 1;
	if (expect_load_failure("BWORLD one\n", "non-numeric version")) return 1;
	if (expect_load_failure("", "empty input")) return 1;
	if (expect_load_failure("# only comments\n", "no header")) return 1;
	if (expect_load_failure("BWORLD 1\nend\n", "unbalanced end")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nend\nend\n", "extra end")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\npos 1 2\n", "EOF inside entity")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nentity b\nend\n", "EOF inside nested entity"))
		return 1;
	if (expect_load_failure("BWORLD 1\nentity\nend\n", "entity without name")) return 1;
	if (expect_load_failure("BWORLD 1\npos 1 2\n", "key outside entity")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\npos 1 nope\nend\n", "bad pos")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\npos 1\nend\n", "short pos")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nrot xyz\nend\n", "bad rot")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nscale 1\nend\n", "short scale")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nvisible 2\nend\n", "bad visible")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nvisible x\nend\n", "non-numeric visible"))
		return 1;
	if (expect_load_failure("BWORLD 1\nentity a\ntint 1 2 3\nend\n", "short tint")) return 1;
	if (expect_load_failure("BWORLD 1\ncamera 1 2 nope\n", "bad camera")) return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nsprite missing.png 0 0 1 1\nend\n",
							"missing texture"))
		return 1;
	if (expect_load_failure("BWORLD 1\nentity a\nsprite bad.png 0 0 1\nend\n", "short sprite"))
		return 1;

	/* A malformed second entity rolls back the first successfully spawned one. */
	if (expect_load_failure("BWORLD 1\nentity good\npos 1 1\nend\nentity bad\npos oops 2\nend\n",
							"rollback"))
		return 1;

	/* NULL arguments are rejected with an error message. */
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	bapi_set_last_error("");
	CHECK(bapi_world_load_from_memory(NULL, "BWORLD 1\n", 9) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_set_last_error("");
	CHECK(bapi_world_load_from_memory(world, NULL, 10) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_set_last_error("");
	CHECK(bapi_world_load_from_memory(world, "BWORLD 1\n", 0) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_world_destroy(world);
	return 0;
}

static int test_existing_world_and_file_errors(void)
{
	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	bapi_entity_t existing = bapi_world_spawn(world, "existing");
	CHECK(existing != NULL);

	bapi_set_last_error("");
	CHECK(bapi_world_load(world, "world_io_test_missing_file.bworld") == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	CHECK(bapi_world_entity_count(world) == 1);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 0)), "existing") == 0);

	/* A failed load leaves pre-existing roots untouched. */
	const char *bad = "BWORLD 1\nentity new\npos oops 1\nend\n";
	CHECK(bapi_world_load_from_memory(world, bad, strlen(bad)) == -1);
	CHECK(bapi_world_entity_count(world) == 1);

	/* A successful load appends new roots. */
	const char *good = "BWORLD 1\ncamera 1 2 3\nentity added\npos 5 6\nend\n";
	CHECK(bapi_world_load_from_memory(world, good, strlen(good)) == 0);
	CHECK(bapi_world_entity_count(world) == 2);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 0)), "existing") == 0);
	CHECK(strcmp(bapi_entity_get_name(bapi_world_get_entity(world, 1)), "added") == 0);

	bapi_world_destroy(world);
	return 0;
}

static int test_sprite_skip_and_save_errors(void)
{
	static struct bapi_texture_internal no_path_texture = {NULL};
	const char						   *path			= "world_io_test_no_sprite.bworld";
	remove(path);

	bapi_world_t world = bapi_world_create();
	CHECK(world != NULL);
	bapi_entity_t entity = bapi_world_spawn(world, "sprite_less");
	CHECK(entity != NULL);
	bapi_entity_set_texture(entity, &no_path_texture, (bapi_rect_t){0.0f, 0.0f, 8.0f, 8.0f});

	CHECK(bapi_world_save(world, path) == 0);
	char *content = read_file(path);
	CHECK(content != NULL);
	CHECK(strstr(content, "sprite ") == NULL);
	free(content);

	bapi_world_t loaded = bapi_world_create();
	CHECK(loaded != NULL);
	CHECK(bapi_world_load(loaded, path) == 0);
	CHECK(bapi_world_entity_count(loaded) == 1);
	CHECK(bapi_entity_get_texture(bapi_world_get_entity(loaded, 0)) == NULL);

	bapi_set_last_error("");
	CHECK(bapi_world_save(NULL, path) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_set_last_error("");
	CHECK(bapi_world_save(world, NULL) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_set_last_error("");
	CHECK(bapi_world_save(world, "") == -1);
	CHECK(bapi_get_last_error()[0] != '\0');

	bapi_set_last_error("");
	CHECK(bapi_world_load(NULL, path) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');
	bapi_set_last_error("");
	CHECK(bapi_world_load(world, NULL) == -1);
	CHECK(bapi_get_last_error()[0] != '\0');

	/* Saving over an existing file replaces it (atomic rename path). */
	CHECK(bapi_world_save(world, path) == 0);
	CHECK(bapi_world_save(world, path) == 0);

	bapi_world_destroy(loaded);
	bapi_world_destroy(world);
	remove(path);
	return 0;
}

int main(void)
{
	remove("world_io_test_roundtrip.bworld");
	remove("world_io_test_roundtrip_2.bworld");
	remove("world_io_test_no_sprite.bworld");

	if (test_roundtrip_file() != 0) return 1;
	if (test_parser_tolerance() != 0) return 1;
	if (test_rejections() != 0) return 1;
	if (test_existing_world_and_file_errors() != 0) return 1;
	if (test_sprite_skip_and_save_errors() != 0) return 1;

	printf("world_io_test: ok\n");
	return 0;
}
