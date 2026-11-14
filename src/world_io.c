#include "BridgeEngine.h"
#include "internal/bapi_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLD_IO_NAME_MAX 63

/* --- load state ---------------------------------------------------------- */

typedef struct {
	bapi_entity_t entity;
	int			  line;
} wio_entity_frame_t;

typedef struct {
	wio_entity_frame_t *frames;
	int					count;
	int					capacity;
} wio_entity_stack_t;

static int wio_stack_push(wio_entity_stack_t *stack, bapi_entity_t entity, int line)
{
	if (stack->count >= stack->capacity) {
		int					new_capacity = stack->capacity == 0 ? 8 : stack->capacity * 2;
		wio_entity_frame_t *grown = realloc(stack->frames, (size_t)new_capacity * sizeof(*grown));
		if (grown == NULL) return -1;
		stack->frames	= grown;
		stack->capacity = new_capacity;
	}
	stack->frames[stack->count].entity = entity;
	stack->frames[stack->count].line   = line;
	stack->count++;
	return 0;
}

/* --- text helpers -------------------------------------------------------- */

static int wio_is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *wio_trim(char *text)
{
	while (wio_is_space(*text)) text++;
	if (*text == '\0') return text;

	size_t length = strlen(text);
	while (length > 0 && wio_is_space(text[length - 1])) {
		text[--length] = '\0';
	}
	return text;
}

/* Split off the next whitespace-delimited token from *cursor, NUL-terminating
 * it in place. Returns NULL when the line is exhausted. */
static char *wio_next_token(char **cursor)
{
	char *start = *cursor;
	while (wio_is_space(*start)) start++;
	if (*start == '\0') {
		*cursor = start;
		return NULL;
	}

	char *end = start;
	while (*end != '\0' && !wio_is_space(*end)) end++;
	if (*end != '\0') {
		*end = '\0';
		end++;
	}
	*cursor = end;
	return start;
}

static int wio_parse_float(const char *token, float *out)
{
	if (token == NULL || token[0] == '\0') return -1;

	char *end	= NULL;
	float value = strtof(token, &end);
	if (end == token || *end != '\0') return -1;
	*out = value;
	return 0;
}

static int wio_parse_int(const char *token, int *out)
{
	if (token == NULL || token[0] == '\0') return -1;

	char *end	= NULL;
	long  value = strtol(token, &end, 10);
	if (end == token || *end != '\0') return -1;
	if (value < INT_MIN || value > INT_MAX) return -1;
	*out = (int)value;
	return 0;
}

static int wio_read_float(char **cursor, float *out)
{
	return wio_parse_float(wio_next_token(cursor), out);
}

static int wio_read_int(char **cursor, int *out)
{
	return wio_parse_int(wio_next_token(cursor), out);
}

/* --- saving -------------------------------------------------------------- */

static int wio_write_entity(FILE *file, bapi_entity_t entity)
{
	const char *name = bapi_entity_get_name(entity);
	if (name == NULL || name[0] == '\0') name = "entity";

	/* bapi_entity_create caps names at 63 bytes; truncate defensively in case
	 * a future entity implementation allows longer names. */
	if (fprintf(file, "entity %.63s\n", name) < 0) return -1;

	bapi_vec2_t position = bapi_entity_get_position(entity);
	if (fprintf(file, "pos %g %g\n", position.x, position.y) < 0) return -1;

	if (fprintf(file, "rot %g\n", bapi_entity_get_rotation(entity)) < 0) return -1;

	bapi_vec2_t scale = bapi_entity_get_scale(entity);
	if (fprintf(file, "scale %g %g\n", scale.x, scale.y) < 0) return -1;

	if (fprintf(file, "visible %d\n", bapi_entity_is_visible(entity) ? 1 : 0) < 0) return -1;

	bapi_texture_t texture = bapi_entity_get_texture(entity);
	if (texture != NULL) {
		const char *path = bapi_texture_get_path(texture);
		/* Memory/atlas-owned textures have no file path and cannot be saved. */
		if (path != NULL && path[0] != '\0') {
			bapi_rect_t source = bapi_entity_get_source(entity);
			if (fprintf(file, "sprite %s %g %g %g %g\n", path, source.x, source.y, source.w,
						source.h) < 0) {
				return -1;
			}
		}
	}

	bapi_color_t tint = bapi_entity_get_tint(entity);
	if (fprintf(file, "tint %d %d %d %d\n", (int)tint.r, (int)tint.g, (int)tint.b, (int)tint.a) <
		0) {
		return -1;
	}

	int child_count = bapi_entity_get_child_count(entity);
	for (int i = 0; i < child_count; i++) {
		if (wio_write_entity(file, bapi_entity_get_child(entity, i)) != 0) return -1;
	}

	return fprintf(file, "end\n") < 0 ? -1 : 0;
}

int bapi_world_save(bapi_world_t world, const char *filepath)
{
	if (world == NULL) {
		bapi_set_last_error("world_save: world is NULL");
		return -1;
	}
	if (filepath == NULL || filepath[0] == '\0') {
		bapi_set_last_error("world_save: file path is empty");
		return -1;
	}

#ifndef USE_BACKEND_XJ380
	/* Write to "<filepath>.tmp" and rename over the destination so a partially
	 * written world file is never observed. */
	size_t path_length = strlen(filepath);
	char  *temp_path   = malloc(path_length + 5);
	if (temp_path == NULL) {
		bapi_set_last_error("world_save: out of memory");
		return -1;
	}
	memcpy(temp_path, filepath, path_length);
	memcpy(temp_path + path_length, ".tmp", 5);

	FILE *file = fopen(temp_path, "wb");
	if (file == NULL) {
		bapi_set_last_error("world_save: cannot open '%s' for writing", temp_path);
		free(temp_path);
		return -1;
	}
#else
	FILE *file = fopen(filepath, "wb");
	if (file == NULL) {
		bapi_set_last_error("world_save: cannot open '%s' for writing", filepath);
		return -1;
	}
#endif

	int ok = 1;
	if (fprintf(file, "BWORLD %d\n", BAPI_SERIALIZE_VERSION) < 0) ok = 0;

	bapi_camera_t camera;
	bapi_world_get_camera(world, &camera);
	if (ok && fprintf(file, "camera %g %g %g\n", camera.x, camera.y, camera.zoom) < 0) ok = 0;

	int root_count = bapi_world_entity_count(world);
	for (int i = 0; ok && i < root_count; i++) {
		if (wio_write_entity(file, bapi_world_get_entity(world, i)) != 0) ok = 0;
	}

	if (ok && fflush(file) != 0) ok = 0;
	if (fclose(file) != 0) ok = 0;

	if (!ok) {
		bapi_set_last_error("world_save: failed while writing '%s'", filepath);
#ifndef USE_BACKEND_XJ380
		remove(temp_path);
		free(temp_path);
#endif
		return -1;
	}

#ifndef USE_BACKEND_XJ380
	if (rename(temp_path, filepath) != 0) {
		/* Some platforms refuse to rename over an existing file. */
		remove(filepath);
		if (rename(temp_path, filepath) != 0) {
			bapi_set_last_error("world_save: cannot replace '%s'", filepath);
			remove(temp_path);
			free(temp_path);
			return -1;
		}
	}
	free(temp_path);
#endif
	return 0;
}

/* --- loading ------------------------------------------------------------- */

static void wio_rollback(bapi_world_t world, int base_count)
{
	while (bapi_world_entity_count(world) > base_count) {
		int index = bapi_world_entity_count(world) - 1;
		bapi_world_despawn(world, bapi_world_get_entity(world, index));
	}
}

static int wio_is_entity_key(const char *key)
{
	return strcmp(key, "pos") == 0 || strcmp(key, "rot") == 0 || strcmp(key, "scale") == 0 ||
		   strcmp(key, "visible") == 0 || strcmp(key, "sprite") == 0 || strcmp(key, "tint") == 0;
}

static int wio_parse_entity_fields(bapi_entity_t entity, const char *key, char *cursor, int line)
{
	if (strcmp(key, "pos") == 0) {
		bapi_vec2_t position;
		if (wio_read_float(&cursor, &position.x) != 0 ||
			wio_read_float(&cursor, &position.y) != 0) {
			bapi_set_last_error("world: invalid 'pos' on line %d", line);
			return -1;
		}
		bapi_entity_set_position(entity, position);
		return 0;
	}

	if (strcmp(key, "rot") == 0) {
		float rotation = 0.0f;
		if (wio_read_float(&cursor, &rotation) != 0) {
			bapi_set_last_error("world: invalid 'rot' on line %d", line);
			return -1;
		}
		bapi_entity_set_rotation(entity, rotation);
		return 0;
	}

	if (strcmp(key, "scale") == 0) {
		bapi_vec2_t scale;
		if (wio_read_float(&cursor, &scale.x) != 0 || wio_read_float(&cursor, &scale.y) != 0) {
			bapi_set_last_error("world: invalid 'scale' on line %d", line);
			return -1;
		}
		bapi_entity_set_scale(entity, scale);
		return 0;
	}

	if (strcmp(key, "visible") == 0) {
		int visible = 0;
		if (wio_read_int(&cursor, &visible) != 0 || (visible != 0 && visible != 1)) {
			bapi_set_last_error("world: invalid 'visible' on line %d", line);
			return -1;
		}
		bapi_entity_set_visible(entity, visible);
		return 0;
	}

	if (strcmp(key, "sprite") == 0) {
		char	   *path = wio_next_token(&cursor);
		bapi_rect_t source;
		if (path == NULL || path[0] == '\0' || wio_read_float(&cursor, &source.x) != 0 ||
			wio_read_float(&cursor, &source.y) != 0 || wio_read_float(&cursor, &source.w) != 0 ||
			wio_read_float(&cursor, &source.h) != 0) {
			bapi_set_last_error("world: invalid 'sprite' on line %d", line);
			return -1;
		}
		bapi_texture_t texture = bapi_texture_load(path);
		if (texture == NULL) {
			bapi_set_last_error("world: cannot load sprite texture '%s' on line %d", path, line);
			return -1;
		}
		bapi_entity_set_texture(entity, texture, source);
		return 0;
	}

	if (strcmp(key, "tint") == 0) {
		int r = 0, g = 0, b = 0, a = 0;
		if (wio_read_int(&cursor, &r) != 0 || wio_read_int(&cursor, &g) != 0 ||
			wio_read_int(&cursor, &b) != 0 || wio_read_int(&cursor, &a) != 0) {
			bapi_set_last_error("world: invalid 'tint' on line %d", line);
			return -1;
		}
		bapi_entity_set_tint(entity,
							 (bapi_color_t){(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
		return 0;
	}

	/* Unknown keys are ignored for forward compatibility. */
	return 1;
}

int bapi_world_load_from_memory(bapi_world_t world, const char *text, size_t size)
{
	if (world == NULL) {
		bapi_set_last_error("world_load: world is NULL");
		return -1;
	}
	if (text == NULL || size == 0) {
		bapi_set_last_error("world_load: input is empty");
		return -1;
	}

	char *buffer = malloc(size + 1);
	if (buffer == NULL) {
		bapi_set_last_error("world_load: out of memory");
		return -1;
	}
	memcpy(buffer, text, size);
	buffer[size] = '\0';

	int base_count	= bapi_world_entity_count(world);
	int result		= -1;
	int line_no		= 0;
	int have_header = 0;

	wio_entity_stack_t stack  = {0};
	bapi_camera_t	   camera = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};

	char *cursor = buffer;
	while (*cursor != '\0') {
		char *line = cursor;
		char *nl   = strchr(cursor, '\n');
		if (nl != NULL) {
			*nl	   = '\0';
			cursor = nl + 1;
		} else {
			cursor += strlen(cursor);
		}
		line_no++;

		char *comment = strchr(line, '#');
		if (comment != NULL) *comment = '\0';

		char *line_text = wio_trim(line);
		if (line_text[0] == '\0') continue;

		char *rest = line_text;
		char *key  = wio_next_token(&rest);
		if (key == NULL) continue;

		if (!have_header) {
			if (strcmp(key, "BWORLD") != 0) {
				bapi_set_last_error("world_load: bad magic '%s' on line %d", key, line_no);
				goto done;
			}
			int version = 0;
			if (wio_read_int(&rest, &version) != 0) {
				bapi_set_last_error("world_load: missing version on line %d", line_no);
				goto done;
			}
			if (version < 1 || version > BAPI_SERIALIZE_VERSION) {
				bapi_set_last_error(
					"world_load: unsupported version %d on line %d (expected 1..%d)", version,
					line_no, BAPI_SERIALIZE_VERSION);
				goto done;
			}
			have_header = 1;
			continue;
		}

		if (strcmp(key, "entity") == 0) {
			char *name = wio_trim(rest);
			if (name[0] == '\0') {
				bapi_set_last_error("world_load: entity is missing a name on line %d", line_no);
				goto done;
			}

			bapi_entity_t entity;
			if (stack.count == 0) {
				entity = bapi_world_spawn(world, name);
			} else {
				entity = bapi_entity_create(name);
				if (entity != NULL &&
					bapi_entity_add_child(stack.frames[stack.count - 1].entity, entity) != 0) {
					bapi_entity_destroy(entity);
					entity = NULL;
				}
			}
			if (entity == NULL) {
				bapi_set_last_error("world_load: cannot create entity on line %d", line_no);
				goto done;
			}
			if (wio_stack_push(&stack, entity, line_no) != 0) {
				bapi_set_last_error("world_load: out of memory on line %d", line_no);
				goto done;
			}
			continue;
		}

		if (strcmp(key, "end") == 0) {
			if (stack.count == 0) {
				bapi_set_last_error("world_load: unbalanced 'end' on line %d", line_no);
				goto done;
			}
			stack.count--;
			continue;
		}

		if (strcmp(key, "camera") == 0) {
			/* camera belongs to the world header; a nested camera line is
			 * ignored so entity keys stay unambiguous. */
			if (stack.count > 0) continue;

			float x = 0.0f, y = 0.0f, zoom = 1.0f;
			if (wio_read_float(&rest, &x) != 0 || wio_read_float(&rest, &y) != 0 ||
				wio_read_float(&rest, &zoom) != 0) {
				bapi_set_last_error("world_load: invalid 'camera' on line %d", line_no);
				goto done;
			}
			camera.x	= x;
			camera.y	= y;
			camera.zoom = zoom;
			continue;
		}

		if (stack.count == 0) {
			/* Known entity fields outside an entity are not valid, but unknown
			 * keys are tolerated for forward compatibility. */
			if (wio_is_entity_key(key)) {
				bapi_set_last_error("world_load: '%s' outside an entity on line %d", key, line_no);
				goto done;
			}
			continue;
		}

		int field_result =
			wio_parse_entity_fields(stack.frames[stack.count - 1].entity, key, rest, line_no);
		if (field_result < 0) goto done;
	}

	if (!have_header) {
		bapi_set_last_error("world_load: missing BWORLD header");
		goto done;
	}
	if (stack.count > 0) {
		bapi_set_last_error("world_load: unexpected end of file inside entity opened on line %d",
							stack.frames[stack.count - 1].line);
		goto done;
	}

	bapi_world_set_camera(world, camera);
	result = 0;

done:
	if (result != 0) wio_rollback(world, base_count);
	free(stack.frames);
	free(buffer);
	return result;
}

int bapi_world_load(bapi_world_t world, const char *filepath)
{
	if (world == NULL) {
		bapi_set_last_error("world_load: world is NULL");
		return -1;
	}
	if (filepath == NULL || filepath[0] == '\0') {
		bapi_set_last_error("world_load: file path is empty");
		return -1;
	}

	FILE *file = fopen(filepath, "rb");
	if (file == NULL) {
		bapi_set_last_error("world_load: cannot open '%s'", filepath);
		return -1;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		bapi_set_last_error("world_load: cannot seek '%s'", filepath);
		fclose(file);
		return -1;
	}
	long file_size = ftell(file);
	if (file_size < 0) {
		bapi_set_last_error("world_load: cannot determine size of '%s'", filepath);
		fclose(file);
		return -1;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		bapi_set_last_error("world_load: cannot rewind '%s'", filepath);
		fclose(file);
		return -1;
	}

	size_t size	  = (size_t)file_size;
	char  *buffer = malloc(size + 1);
	if (buffer == NULL) {
		bapi_set_last_error("world_load: out of memory");
		fclose(file);
		return -1;
	}
	if (size > 0 && fread(buffer, 1, size, file) != size) {
		bapi_set_last_error("world_load: failed while reading '%s'", filepath);
		free(buffer);
		fclose(file);
		return -1;
	}
	buffer[size] = '\0';
	fclose(file);

	int result = bapi_world_load_from_memory(world, buffer, size);
	free(buffer);
	return result;
}
