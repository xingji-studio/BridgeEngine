/* Self-contained tests for .btm tilemap serialization.
 *
 * Links src/tilemap_io.c, src/tilemap.c and src/error.c. File IO and the
 * tileset texture loader are mocked so the test controls the bytes exactly and
 * needs no platform backend or real image assets.
 *
 * Build/run:
 *   cmake --build build --target bridgeengine_tilemap_io_test
 *   ./build/bridgeengine_tilemap_io_test
 */

#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/texture_internal.h"
#include "internal/engine/tilemap_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PATH "/tmp/bridgeengine_tilemap_io_test.btm"

/* ------------------------------------------------------------------------- */
/* Mocks for the dependencies of tilemap_io.c / tilemap.c                    */
/* ------------------------------------------------------------------------- */

/* Uses the real texture layout so the test stays valid if src/texture.c is ever
 * linked into this target; only cache_key (the source path) drives save(). */
static struct bapi_texture_internal g_fake_texture;
static int							g_texture_load_count;
static int							g_texture_destroy_count;
static int							g_texture_load_fails;
static char							g_loaded_texture_path[256];

bapi_texture_t bapi_texture_load(const char *filepath)
{
	g_texture_load_count++;
	snprintf(g_loaded_texture_path, sizeof(g_loaded_texture_path), "%s",
			 filepath != NULL ? filepath : "");
	if (g_texture_load_fails) return NULL;
	g_fake_texture.cache_key = g_loaded_texture_path;
	return &g_fake_texture;
}

void bapi_texture_destroy(bapi_texture_t texture)
{
	(void)texture;
	g_texture_destroy_count++;
}

const char *bapi_texture_get_path(bapi_texture_t texture)
{
	return texture != NULL ? texture->cache_key : NULL;
}

/* Read a file through the engine's IO entry point; the real one lives in
 * src/io.c which is not part of this test target. */
uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) return NULL;

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long length = ftell(file);
	if (length < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	size_t	 size	= (size_t)length;
	uint8_t *buffer = malloc(size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	if (size > 0 && fread(buffer, 1, size, file) != size) {
		free(buffer);
		fclose(file);
		return NULL;
	}
	fclose(file);

	buffer[size] = '\0'; /* convenient, though the parser uses size */
	if (out_size != NULL) *out_size = size;
	return buffer;
}

/* tilemap.c references the draw backend even though these tests never draw. */
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color)
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)color;
}

void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params)
{
	(void)texture;
	(void)params;
}

/* ------------------------------------------------------------------------- */
/* Test helpers                                                              */
/* ------------------------------------------------------------------------- */

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "FAIL: %s\n", message);
	return condition ? 0 : 1;
}

static int expect_error_contains(const char *needle, const char *message)
{
	const char *error = bapi_get_last_error();
	if (error != NULL && strstr(error, needle) != NULL) return 0;
	fprintf(stderr, "FAIL: %s (last error: '%s', expected substring '%s')\n", message,
			error != NULL ? error : "(null)", needle);
	return 1;
}

static bapi_tilemap_t load_text(const char *text)
{
	return bapi_tilemap_load_from_memory(text, strlen(text));
}

static char *read_file_text(const char *path, size_t *out_size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) return NULL;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}
	char *buffer = malloc((size_t)length + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	size_t read = length > 0 ? fread(buffer, 1, (size_t)length, file) : 0;
	fclose(file);
	if (read != (size_t)length) {
		free(buffer);
		return NULL;
	}
	buffer[length] = '\0';
	if (out_size != NULL) *out_size = (size_t)length;
	return buffer;
}

static void reset_texture_mocks(void)
{
	g_texture_load_count	 = 0;
	g_texture_destroy_count	 = 0;
	g_texture_load_fails	 = 0;
	g_loaded_texture_path[0] = '\0';
	g_fake_texture.cache_key = NULL;
}

static int file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) return 0;
	fclose(file);
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static int test_roundtrip(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(4, 3, 16, 24);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	bapi_tilemap_set_tile(map, 0, 0, 0, 0);
	bapi_tilemap_set_tile(map, 1, 0, 7, 1);
	bapi_tilemap_set_tile(map, 2, 0, -1, 0);
	bapi_tilemap_set_tile(map, 3, 0, 3, 0);
	bapi_tilemap_set_tile(map, 0, 1, 12, 1);
	bapi_tilemap_set_tile(map, 1, 1, -1, 0);
	bapi_tilemap_set_tile(map, 2, 2, 255, 1);

	remove(TEST_PATH);
	remove(TEST_PATH ".tmp");

	result |= expect(bapi_tilemap_save(map, TEST_PATH) == 0, "save succeeds");
	result |= expect(!file_exists(TEST_PATH ".tmp"), "save removes its temp file");

	/* Saving again must atomically replace the previous file. */
	result |= expect(bapi_tilemap_save(map, TEST_PATH) == 0, "save overwrites an existing file");

	size_t text_size = 0;
	char  *text		 = read_file_text(TEST_PATH, &text_size);
	result |= expect(text != NULL && text_size > 0, "saved file is readable");
	if (text != NULL) {
		result |= expect(strncmp(text, "BTM 1\n", 6) == 0, "file starts with the version header");
		result |= expect(strstr(text, "size 4 3\n") != NULL, "file stores the grid size");
		result |= expect(strstr(text, "tile 16 24\n") != NULL, "file stores the tile size");
		result |= expect(strstr(text, "\ntiles\n") != NULL, "file has a tiles block");
		result |= expect(strstr(text, "\nsolid\n") != NULL, "file has a solid block");
		result |= expect(strstr(text, "tileset") == NULL, "tileset line omitted when unset");
		free(text);
	}

	bapi_tilemap_t loaded = bapi_tilemap_load(TEST_PATH);
	result |= expect(loaded != NULL, "load succeeds");
	if (loaded != NULL) {
		result |= expect(loaded->width == 4 && loaded->height == 3, "grid size roundtrips");
		result |=
			expect(loaded->tile_width == 16 && loaded->tile_height == 24, "tile size roundtrips");
		result |= expect(loaded->tileset == NULL, "no tileset roundtrips");

		int equal = 1;
		for (int y = 0; y < 3; y++) {
			for (int x = 0; x < 4; x++) {
				int index = y * 4 + x;
				if (loaded->tiles[index] != map->tiles[index]) equal = 0;
				if ((loaded->solid[index] != 0) != (map->solid[index] != 0)) equal = 0;
			}
		}
		result |= expect(equal, "all tile ids and solid flags roundtrip");
		bapi_tilemap_destroy(loaded);
	}

	bapi_tilemap_destroy(map);
	remove(TEST_PATH);
	remove(TEST_PATH ".tmp");
	return result;
}

static int test_literal_file(void)
{
	int result = 0;

	/* Comments, blank lines and unknown keys are ignored. */
	const char *literal = "# hand written tilemap\n"
						  "BTM 1\n"
						  "\n"
						  "size 3 2\n"
						  "tile 8 8\n"
						  "author someone\n"
						  "tiles\n"
						  "0 1 2\n"
						  "3 4 -1\n"
						  "solid\n"
						  "0 0 0\n"
						  "0 1 0\n";

	bapi_tilemap_t map = load_text(literal);
	result |= expect(map != NULL, "hand written file loads");
	if (map != NULL) {
		result |= expect(map->width == 3 && map->height == 2, "literal grid size parsed");
		result |= expect(map->tile_width == 8 && map->tile_height == 8, "literal tile size parsed");

		const int expected_tiles[6] = {0, 1, 2, 3, 4, -1};
		const int expected_solid[6] = {0, 0, 0, 0, 1, 0};
		int		  equal				= 1;
		for (int index = 0; index < 6; index++) {
			if (map->tiles[index] != expected_tiles[index]) equal = 0;
			if ((map->solid[index] != 0) != (expected_solid[index] != 0)) equal = 0;
		}
		result |= expect(equal, "literal ids and solid flags parsed row-major");
		result |= expect(map->tileset == NULL, "literal without tileset has no texture");

		bapi_tilemap_destroy(map);
	}

	return result;
}

static int test_missing_solid_defaults_to_zero(void)
{
	int result = 0;

	const char *literal = "BTM 1\n"
						  "size 2 2\n"
						  "tile 8 8\n"
						  "tiles\n"
						  "1 2\n"
						  "3 4\n";

	bapi_tilemap_t map = load_text(literal);
	result |= expect(map != NULL, "file without a solid block loads");
	if (map != NULL) {
		int all_solid = 0;
		for (int index = 0; index < 4; index++) all_solid += map->solid[index] != 0;
		result |= expect(all_solid == 0, "missing solid block defaults to non-solid");
		result |= expect(map->tiles[0] == 1 && map->tiles[3] == 4, "tiles still load");
		bapi_tilemap_destroy(map);
	}

	return result;
}

static int test_tileset_roundtrip(void)
{
	int result = 0;

	reset_texture_mocks();
	const char *literal = "BTM 1\n"
						  "size 2 2\n"
						  "tile 8 8\n"
						  "tileset assets/tiles.png 4\n"
						  "tiles\n"
						  "0 1\n"
						  "2 3\n"
						  "solid\n"
						  "0 0\n"
						  "0 0\n";

	bapi_tilemap_t map = load_text(literal);
	result |= expect(map != NULL, "tileset file loads");
	result |= expect(g_texture_load_count == 1, "tileset texture loaded once");
	result |= expect(strcmp(g_loaded_texture_path, "assets/tiles.png") == 0,
					 "tileset path passed to bapi_texture_load");
	if (map != NULL) {
		result |= expect(map->tileset == &g_fake_texture, "loaded texture is attached");
		result |= expect(map->columns == 4, "tileset columns stored");

		remove(TEST_PATH);
		result |= expect(bapi_tilemap_save(map, TEST_PATH) == 0, "map with tileset saves");
		size_t text_size = 0;
		char  *text		 = read_file_text(TEST_PATH, &text_size);
		result |= expect(text != NULL, "saved tileset file is readable");
		if (text != NULL) {
			result |= expect(strstr(text, "tileset assets/tiles.png 4\n") != NULL,
							 "tileset line is written");
			free(text);
		}

		bapi_tilemap_t reloaded = bapi_tilemap_load(TEST_PATH);
		result |= expect(reloaded != NULL, "saved tileset map reloads");
		if (reloaded != NULL) {
			result |= expect(reloaded->tileset == &g_fake_texture,
							 "tileset texture is re-attached on load");
			result |= expect(reloaded->columns == 4, "columns roundtrip");
			bapi_tilemap_destroy(reloaded);
		}
		bapi_tilemap_destroy(map);
		remove(TEST_PATH);
		remove(TEST_PATH ".tmp");
	}

	/* A pathless texture cannot be re-opened, so it is not serialized. */
	reset_texture_mocks();
	bapi_tilemap_t pathless = bapi_tilemap_create(1, 1, 8, 8);
	if (pathless != NULL) {
		g_fake_texture.cache_key = NULL;
		bapi_tilemap_set_tileset(pathless, &g_fake_texture, 2);
		remove(TEST_PATH);
		result |= expect(bapi_tilemap_save(pathless, TEST_PATH) == 0, "pathless tileset saves");
		char *text = read_file_text(TEST_PATH, NULL);
		result |= expect(text != NULL && strstr(text, "tileset") == NULL,
						 "pathless tileset omits the tileset line");
		free(text);
		bapi_tilemap_t reloaded = bapi_tilemap_load(TEST_PATH);
		result |= expect(reloaded != NULL && reloaded->tileset == NULL,
						 "pathless saved map reloads without a tileset");
		bapi_tilemap_destroy(reloaded);
		bapi_tilemap_destroy(pathless);
		remove(TEST_PATH);
	}

	/* A failed texture load fails the whole load with a concrete message. */
	reset_texture_mocks();
	g_texture_load_fails  = 1;
	bapi_tilemap_t failed = load_text(literal);
	result |= expect(failed == NULL, "tileset texture failure fails the load");
	result |= expect_error_contains("tileset", "tileset failure reports the tileset");
	reset_texture_mocks();

	return result;
}

static int test_non_nul_terminated_buffer(void)
{
	int result = 0;

	const char *literal = "BTM 1\n"
						  "size 2 1\n"
						  "tile 8 8\n"
						  "tiles\n"
						  "4 5\n"
						  "solid\n"
						  "1 0\n";
	size_t		length	= strlen(literal);
	char	   *buffer	= malloc(length); /* exactly the bytes, no terminator */
	if (buffer == NULL) return expect(0, "allocation failed");
	memcpy(buffer, literal, length);

	bapi_tilemap_t map = bapi_tilemap_load_from_memory(buffer, length);
	result |= expect(map != NULL, "non-NUL-terminated buffer loads");
	if (map != NULL) {
		result |= expect(map->width == 2 && map->height == 1, "non-NUL buffer grid parsed");
		result |= expect(map->tiles[0] == 4 && map->tiles[1] == 5, "non-NUL buffer tiles parsed");
		result |= expect(map->solid[0] == 1 && map->solid[1] == 0, "non-NUL buffer solids parsed");
		bapi_tilemap_destroy(map);
	}

	free(buffer);
	return result;
}

typedef struct {
	const char *name;
	const char *text;
	const char *error_needle;
} reject_case_t;

static int test_rejections(void)
{
	int result = 0;

	static const reject_case_t cases[] = {
		{"bad magic", "NOPE 1\nsize 2 2\ntile 8 8\ntiles\n0 0\n0 1\n", "magic"},
		{"non-numeric version", "BTM x\nsize 2 2\ntile 8 8\ntiles\n", "version"},
		{"version 0", "BTM 0\nsize 2 2\ntile 8 8\ntiles\n0 0\n0 0\n", "version"},
		{"version 99", "BTM 99\nsize 2 2\ntile 8 8\ntiles\n0 0\n0 0\n", "version"},
		{"missing size", "BTM 1\ntile 8 8\ntiles\n0 0\n0 0\n", "size"},
		{"missing tile", "BTM 1\nsize 2 2\ntiles\n0 0\n0 0\n", "tile"},
		{"missing tiles block", "BTM 1\nsize 2 2\ntile 8 8\n", "tiles"},
		{"truncated tiles", "BTM 1\nsize 2 2\ntile 8 8\ntiles\n0 1\n", "tile id"},
		{"too many tiles", "BTM 1\nsize 2 2\ntile 8 8\ntiles\n0 1 2 3 4\nsolid\n0 0 0 0\n",
		 "unexpected integer"},
		{"too many solid flags", "BTM 1\nsize 2 2\ntile 8 8\ntiles\n0 1 2 3\nsolid\n0 0 0 0 1\n",
		 "unexpected integer"},
		{"solid before tiles", "BTM 1\nsize 2 2\ntile 8 8\nsolid\n0 0 0 0\ntiles\n0 0 0 0\n",
		 "solid"},
		{"invalid solid value", "BTM 1\nsize 2 2\ntile 8 8\ntiles\n0 1 2 3\nsolid\n0 2 0 0\n",
		 "solid flag"},
		{"zero width", "BTM 1\nsize 0 2\ntile 8 8\ntiles\n", "size"},
		{"zero tile size", "BTM 1\nsize 2 2\ntile 0 8\ntiles\n", "tile size"},
		{"duplicate tiles", "BTM 1\nsize 2 2\ntile 8 8\ntiles\n0 1\n2 3\ntiles\n0 1\n2 3\n",
		 "duplicate"},
	};

	for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
		bapi_clear_last_error();
		bapi_tilemap_t map = load_text(cases[index].text);
		result |= expect(map == NULL, cases[index].name);
		if (map == NULL) {
			result |= expect_error_contains(cases[index].error_needle, cases[index].name);
		} else {
			bapi_tilemap_destroy(map);
		}
	}

	/* Empty / NULL inputs. */
	bapi_clear_last_error();
	result |= expect(bapi_tilemap_load_from_memory(NULL, 8) == NULL, "NULL text is rejected");
	result |= expect_error_contains("NULL", "NULL text reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_load_from_memory("BTM 1", 0) == NULL, "zero size is rejected");
	result |= expect_error_contains("empty", "zero size reports an error");

	return result;
}

static int test_api_validation(void)
{
	int			   result = 0;
	bapi_tilemap_t map	  = bapi_tilemap_create(2, 2, 8, 8);
	if (map == NULL) return expect(0, "tilemap allocation failed");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_save(NULL, TEST_PATH) == -1, "save rejects a NULL tilemap");
	result |= expect_error_contains("NULL", "NULL tilemap reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_save(map, NULL) == -1, "save rejects a NULL filepath");
	result |= expect_error_contains("filepath", "NULL filepath reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_save(map, "") == -1, "save rejects an empty filepath");
	result |= expect_error_contains("filepath", "empty filepath reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_load(NULL) == NULL, "load rejects a NULL filepath");
	result |= expect_error_contains("filepath", "load NULL filepath reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_load("") == NULL, "load rejects an empty filepath");
	result |= expect_error_contains("filepath", "load empty filepath reports an error");

	bapi_clear_last_error();
	result |= expect(bapi_tilemap_load("/tmp/bridgeengine_missing_btm_asset.btm") == NULL,
					 "load rejects a missing file");
	result |= expect_error_contains("could not read", "missing file reports an error");

	bapi_tilemap_destroy(map);
	return result;
}

int main(void)
{
	int result = 0;

	remove(TEST_PATH);
	remove(TEST_PATH ".tmp");

	result |= test_roundtrip();
	result |= test_literal_file();
	result |= test_missing_solid_defaults_to_zero();
	result |= test_tileset_roundtrip();
	result |= test_non_nul_terminated_buffer();
	result |= test_rejections();
	result |= test_api_validation();

	reset_texture_mocks();
	remove(TEST_PATH);
	remove(TEST_PATH ".tmp");

	if (result == 0) {
		printf("tilemap io tests passed\n");
		return 0;
	}

	fprintf(stderr, "tilemap io tests failed\n");
	return 1;
}
