/* .btm tilemap serialization (text format, version 1).
 *
 * Canonical layout written by bapi_tilemap_save:
 *
 *   BTM 1
 *   size <width> <height>
 *   tile <tile_width> <tile_height>
 *   tileset <path> <columns>     (only when the map has a path-backed tileset)
 *   tiles
 *   <width ints per row, one row per line, row-major, -1 = empty>
 *   solid
 *   <width 0/1 per row, one row per line>
 *
 * The parser ignores '#' comments to end of line, blank lines and unknown
 * non-numeric keys, so newer files can carry extra metadata. A missing `solid`
 * block means all tiles are non-solid. Unknown *numeric* tokens are rejected,
 * which catches an over-long `tiles` or `solid` block instead of silently
 * ignoring the extra data.
 *
 * Tileset paths are single tokens (no whitespace, no '#'), matching the saved
 * format. A texture without a source path (memory/atlas-owned) is not
 * serialized: the map then reloads without a tileset rather than with a
 * reference that bapi_texture_load() could never resolve.
 *
 * Ownership: bapi_tilemap_load()/_from_memory() create the tileset texture via
 * bapi_texture_load(). The tilemap borrows the texture (see bapi_tilemap_set_
 * tileset), so the returned map keeps it alive only for its own lifetime; the
 * caller that wants to free it must keep the bapi_texture_t handle. This is the
 * same borrowed-texture contract as the rest of the tilemap API.
 */

#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/tilemap_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BTM_ERROR_PREFIX "bapi_tilemap: "

/* ------------------------------------------------------------------------- */
/* Small token lexer over a byte range (the buffer need not be NUL-terminated) */
/* ------------------------------------------------------------------------- */

typedef struct {
	const char *text;
	size_t		size;
	size_t		pos;
	int			line;
} btm_lexer_t;

static void btm_lexer_init(btm_lexer_t *lexer, const char *text, size_t size)
{
	lexer->text = text;
	lexer->size = size;
	lexer->pos	= 0;
	lexer->line = 1;
}

/* Returns 1 with a token slice, 0 at end of input. '#' starts a comment. */
static int btm_lexer_next(btm_lexer_t *lexer, const char **token, size_t *token_len, int *line)
{
	for (;;) {
		if (lexer->pos >= lexer->size) return 0;
		char current = lexer->text[lexer->pos];
		if (current == '#') {
			while (lexer->pos < lexer->size && lexer->text[lexer->pos] != '\n') lexer->pos++;
			continue;
		}
		if (current == ' ' || current == '\t' || current == '\r' || current == '\n') {
			if (current == '\n') lexer->line++;
			lexer->pos++;
			continue;
		}
		break;
	}

	*line		 = lexer->line;
	size_t start = lexer->pos;
	while (lexer->pos < lexer->size) {
		char current = lexer->text[lexer->pos];
		if (current == ' ' || current == '\t' || current == '\r' || current == '\n' ||
			current == '#') {
			break;
		}
		lexer->pos++;
	}
	*token	   = lexer->text + start;
	*token_len = lexer->pos - start;
	return 1;
}

static int btm_token_equals(const char *token, size_t token_len, const char *literal)
{
	size_t literal_len = strlen(literal);
	return token_len == literal_len && memcmp(token, literal, token_len) == 0;
}

/* Integer token parser that never reads past token_len. */
static int btm_token_to_int(const char *token, size_t token_len, int *out)
{
	size_t index	= 0;
	int	   negative = 0;

	if (token_len == 0) return -1;
	if (token[0] == '-') {
		negative = 1;
		index	 = 1;
	}
	if (index >= token_len) return -1;

	long long		value = 0;
	const long long limit = negative ? 2147483648LL : 2147483647LL;
	for (; index < token_len; index++) {
		if (token[index] < '0' || token[index] > '9') return -1;
		value = value * 10 + (token[index] - '0');
		if (value > limit) return -1;
	}

	*out = negative ? (int)(-value) : (int)value;
	return 0;
}

static int btm_token_is_integer(const char *token, size_t token_len)
{
	int ignored = 0;
	return btm_token_to_int(token, token_len, &ignored) == 0;
}

static int btm_error_precision(size_t token_len)
{
	return token_len > 64 ? 64 : (int)token_len;
}

/* Reads the next token and converts it to an int, reporting the line on error. */
static int btm_read_int(btm_lexer_t *lexer, const char *what, int *out, int *out_line)
{
	const char *token;
	size_t		token_len;
	int			line;

	if (!btm_lexer_next(lexer, &token, &token_len, &line)) {
		bapi_set_last_error(BTM_ERROR_PREFIX "missing %s at end of file", what);
		return -1;
	}
	if (btm_token_to_int(token, token_len, out) != 0) {
		bapi_set_last_error(BTM_ERROR_PREFIX "invalid integer '%.*s' for %s at line %d",
							btm_error_precision(token_len), token, what, line);
		return -1;
	}
	if (out_line != NULL) *out_line = line;
	return 0;
}

static char *btm_copy_token(const char *token, size_t token_len)
{
	char *copy = malloc(token_len + 1);
	if (copy == NULL) return NULL;
	memcpy(copy, token, token_len);
	copy[token_len] = '\0';
	return copy;
}

/* ------------------------------------------------------------------------- */
/* Save                                                                      */
/* ------------------------------------------------------------------------- */

static int btm_write_body(FILE *file, bapi_tilemap_t tilemap)
{
	if (fprintf(file, "BTM %d\n", BAPI_SERIALIZE_VERSION) < 0) return -1;
	if (fprintf(file, "size %d %d\n", tilemap->width, tilemap->height) < 0) return -1;
	if (fprintf(file, "tile %d %d\n", tilemap->tile_width, tilemap->tile_height) < 0) {
		return -1;
	}

	if (tilemap->tileset != NULL && tilemap->columns > 0) {
		const char *tileset_path = bapi_texture_get_path(tilemap->tileset);
		if (tileset_path != NULL && tileset_path[0] != '\0') {
			if (fprintf(file, "tileset %s %d\n", tileset_path, tilemap->columns) < 0) {
				return -1;
			}
		}
	}

	if (fprintf(file, "tiles\n") < 0) return -1;
	for (int y = 0; y < tilemap->height; y++) {
		for (int x = 0; x < tilemap->width; x++) {
			if (x > 0 && fprintf(file, " ") < 0) return -1;
			if (fprintf(file, "%d", tilemap->tiles[y * tilemap->width + x]) < 0) return -1;
		}
		if (fprintf(file, "\n") < 0) return -1;
	}

	if (fprintf(file, "solid\n") < 0) return -1;
	for (int y = 0; y < tilemap->height; y++) {
		for (int x = 0; x < tilemap->width; x++) {
			if (x > 0 && fprintf(file, " ") < 0) return -1;
			if (fprintf(file, "%d", tilemap->solid[y * tilemap->width + x] ? 1 : 0) < 0) {
				return -1;
			}
		}
		if (fprintf(file, "\n") < 0) return -1;
	}

	return 0;
}

int bapi_tilemap_save(bapi_tilemap_t tilemap, const char *filepath)
{
	if (tilemap == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "save called with a NULL tilemap");
		return -1;
	}
	if (filepath == NULL || filepath[0] == '\0') {
		bapi_set_last_error(BTM_ERROR_PREFIX "save called with an empty filepath");
		return -1;
	}
	if (tilemap->width <= 0 || tilemap->height <= 0 || tilemap->tiles == NULL ||
		tilemap->solid == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "save called with an empty grid");
		return -1;
	}

#ifdef USE_BACKEND_XJ380
	/* The XJ380 sysroot has no remove/rename declarations; write in place. */
	FILE *file = fopen(filepath, "wb");
	if (file == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "could not open '%s' for writing", filepath);
		return -1;
	}
#else
	/* Write to "<filepath>.tmp" then rename over the destination so a partially
	 * written .btm is never observed. */
	size_t path_len	 = strlen(filepath);
	char  *temp_path = malloc(path_len + 5);
	if (temp_path == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "out of memory building the temp path");
		return -1;
	}
	memcpy(temp_path, filepath, path_len);
	memcpy(temp_path + path_len, ".tmp", 5);

	FILE *file = fopen(temp_path, "wb");
	if (file == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "could not open '%s' for writing", temp_path);
		free(temp_path);
		return -1;
	}
#endif

	int ok = btm_write_body(file, tilemap) == 0;
	if (ok && fflush(file) != 0) ok = 0;
	if (fclose(file) != 0) ok = 0;

	if (!ok) {
		bapi_set_last_error(BTM_ERROR_PREFIX "failed to write '%s'", filepath);
#ifndef USE_BACKEND_XJ380
		remove(temp_path);
		free(temp_path);
#endif
		return -1;
	}

#ifndef USE_BACKEND_XJ380
	if (rename(temp_path, filepath) != 0) {
		/* Some platforms refuse to rename over an existing file; fall back to
		 * remove + rename instead of leaving the stale tilemap in place. */
		remove(filepath);
		if (rename(temp_path, filepath) != 0) {
			bapi_set_last_error(BTM_ERROR_PREFIX "could not replace '%s'", filepath);
			remove(temp_path);
			free(temp_path);
			return -1;
		}
	}
	free(temp_path);
#endif

	return 0;
}

/* ------------------------------------------------------------------------- */
/* Load                                                                      */
/* ------------------------------------------------------------------------- */

bapi_tilemap_t bapi_tilemap_load_from_memory(const char *text, size_t size)
{
	if (text == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "load_from_memory called with NULL text");
		return NULL;
	}
	if (size == 0) {
		bapi_set_last_error(BTM_ERROR_PREFIX "empty input (no BTM header)");
		return NULL;
	}

	btm_lexer_t lexer;
	btm_lexer_init(&lexer, text, size);

	const char *token;
	size_t		token_len;
	int			line;

	/* Magic + version. */
	if (!btm_lexer_next(&lexer, &token, &token_len, &line)) {
		bapi_set_last_error(BTM_ERROR_PREFIX "empty input (no BTM header)");
		return NULL;
	}
	if (!btm_token_equals(token, token_len, "BTM")) {
		bapi_set_last_error(BTM_ERROR_PREFIX "expected 'BTM' magic, got '%.*s' at line %d",
							btm_error_precision(token_len), token, line);
		return NULL;
	}

	int version = 0;
	if (btm_read_int(&lexer, "version", &version, &line) != 0) return NULL;
	if (version < 1 || version > BAPI_SERIALIZE_VERSION) {
		bapi_set_last_error(BTM_ERROR_PREFIX
							"unsupported .btm version %d at line %d (expected 1..%d)",
							version, line, BAPI_SERIALIZE_VERSION);
		return NULL;
	}

	int width		 = 0;
	int height		 = 0;
	int tile_width	 = 0;
	int tile_height	 = 0;
	int have_size	 = 0;
	int have_tile	 = 0;
	int have_tiles	 = 0;
	int have_solid	 = 0;
	int tileset_line = 0;

	char		  *tileset_path	   = NULL;
	int			   tileset_columns = 0;
	bapi_tilemap_t map			   = NULL;

	while (btm_lexer_next(&lexer, &token, &token_len, &line)) {
		if (btm_token_equals(token, token_len, "size")) {
			if (have_size) {
				bapi_set_last_error(BTM_ERROR_PREFIX "duplicate 'size' at line %d", line);
				goto fail;
			}
			int next_width	= 0;
			int next_height = 0;
			if (btm_read_int(&lexer, "width", &next_width, NULL) != 0) goto fail;
			if (btm_read_int(&lexer, "height", &next_height, NULL) != 0) goto fail;
			if (next_width <= 0 || next_height <= 0) {
				bapi_set_last_error(BTM_ERROR_PREFIX "size must be positive at line %d", line);
				goto fail;
			}
			width	  = next_width;
			height	  = next_height;
			have_size = 1;
		} else if (btm_token_equals(token, token_len, "tile")) {
			if (have_tile) {
				bapi_set_last_error(BTM_ERROR_PREFIX "duplicate 'tile' at line %d", line);
				goto fail;
			}
			int next_width	= 0;
			int next_height = 0;
			if (btm_read_int(&lexer, "tile width", &next_width, NULL) != 0) goto fail;
			if (btm_read_int(&lexer, "tile height", &next_height, NULL) != 0) goto fail;
			if (next_width <= 0 || next_height <= 0) {
				bapi_set_last_error(BTM_ERROR_PREFIX "tile size must be positive at line %d", line);
				goto fail;
			}
			tile_width	= next_width;
			tile_height = next_height;
			have_tile	= 1;
		} else if (btm_token_equals(token, token_len, "tileset")) {
			const char *path_token;
			size_t		path_len;
			int			path_line;
			if (!btm_lexer_next(&lexer, &path_token, &path_len, &path_line)) {
				bapi_set_last_error(
					BTM_ERROR_PREFIX "missing tileset path after 'tileset' at line %d", line);
				goto fail;
			}
			int next_columns = 0;
			if (btm_read_int(&lexer, "tileset columns", &next_columns, NULL) != 0) goto fail;
			if (next_columns <= 0) {
				bapi_set_last_error(BTM_ERROR_PREFIX "tileset columns must be positive at line %d",
									path_line);
				goto fail;
			}

			char *path_copy = btm_copy_token(path_token, path_len);
			if (path_copy == NULL) {
				bapi_set_last_error(BTM_ERROR_PREFIX "out of memory reading the tileset path");
				goto fail;
			}
			free(tileset_path); /* last 'tileset' line wins */
			tileset_path	= path_copy;
			tileset_columns = next_columns;
			tileset_line	= line;
		} else if (btm_token_equals(token, token_len, "tiles")) {
			if (!have_size) {
				bapi_set_last_error(BTM_ERROR_PREFIX "'tiles' before 'size' at line %d", line);
				goto fail;
			}
			if (!have_tile) {
				bapi_set_last_error(BTM_ERROR_PREFIX "'tiles' before 'tile' at line %d", line);
				goto fail;
			}
			if (have_tiles) {
				bapi_set_last_error(BTM_ERROR_PREFIX "duplicate 'tiles' block at line %d", line);
				goto fail;
			}

			long long cell_count = (long long)width * (long long)height;
			if (cell_count <= 0 || cell_count > 2147483647LL) {
				bapi_set_last_error(BTM_ERROR_PREFIX "grid is too large at line %d", line);
				goto fail;
			}

			map = bapi_tilemap_create(width, height, tile_width, tile_height);
			if (map == NULL) {
				bapi_set_last_error(BTM_ERROR_PREFIX "could not allocate the tilemap");
				goto fail;
			}

			for (long long index = 0; index < cell_count; index++) {
				if (btm_read_int(&lexer, "tile id", &map->tiles[index], NULL) != 0) goto fail;
			}
			have_tiles = 1;
		} else if (btm_token_equals(token, token_len, "solid")) {
			if (!have_tiles) {
				bapi_set_last_error(BTM_ERROR_PREFIX "'solid' before 'tiles' at line %d", line);
				goto fail;
			}
			if (have_solid) {
				bapi_set_last_error(BTM_ERROR_PREFIX "duplicate 'solid' block at line %d", line);
				goto fail;
			}

			long long cell_count = (long long)width * (long long)height;
			for (long long index = 0; index < cell_count; index++) {
				int flag	  = 0;
				int flag_line = line;
				if (btm_read_int(&lexer, "solid flag", &flag, &flag_line) != 0) goto fail;
				if (flag != 0 && flag != 1) {
					bapi_set_last_error(BTM_ERROR_PREFIX
										"solid flag must be 0 or 1, got %d at line %d",
										flag, flag_line);
					goto fail;
				}
				map->solid[index] = (uint8_t)flag;
			}
			have_solid = 1;
		} else if (btm_token_is_integer(token, token_len)) {
			/* A bare number outside a block means the previous block had too
			 * many values; reject instead of ignoring the extra data. */
			bapi_set_last_error(BTM_ERROR_PREFIX "unexpected integer '%.*s' at line %d",
								btm_error_precision(token_len), token, line);
			goto fail;
		}
		/* Any other token is an unknown key: ignored for forward compatibility. */
	}

	if (!have_size) {
		bapi_set_last_error(BTM_ERROR_PREFIX "missing 'size' line");
		goto fail;
	}
	if (!have_tile) {
		bapi_set_last_error(BTM_ERROR_PREFIX "missing 'tile' line");
		goto fail;
	}
	if (!have_tiles) {
		bapi_set_last_error(BTM_ERROR_PREFIX "missing 'tiles' block");
		goto fail;
	}

	if (tileset_path != NULL) {
		bapi_texture_t texture = bapi_texture_load(tileset_path);
		if (texture == NULL) {
			bapi_set_last_error(BTM_ERROR_PREFIX "tileset '%s' (line %d) could not be loaded",
								tileset_path, tileset_line);
			goto fail;
		}
		if (bapi_tilemap_set_tileset(map, texture, tileset_columns) != 0) {
			/* set_tileset validates its inputs; this is defensive. */
			bapi_set_last_error(BTM_ERROR_PREFIX "invalid tileset columns %d at line %d",
								tileset_columns, tileset_line);
			bapi_texture_destroy(texture);
			goto fail;
		}
	}

	free(tileset_path);
	return map;

fail:
	if (map != NULL) bapi_tilemap_destroy(map);
	free(tileset_path);
	return NULL;
}

bapi_tilemap_t bapi_tilemap_load(const char *filepath)
{
	if (filepath == NULL || filepath[0] == '\0') {
		bapi_set_last_error(BTM_ERROR_PREFIX "load called with an empty filepath");
		return NULL;
	}

	size_t	 size = 0;
	uint8_t *data = bapi_file_read_alloc(filepath, &size);
	if (data == NULL) {
		bapi_set_last_error(BTM_ERROR_PREFIX "could not read '%s'", filepath);
		return NULL;
	}

	bapi_tilemap_t map = bapi_tilemap_load_from_memory((const char *)data, size);
	free(data);
	return map;
}
