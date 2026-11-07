#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/engine/texture_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bapi_atlas_internal {
	bapi_texture_t		 texture;
	int					 owns_texture;
	char				*page_path;
	bapi_atlas_region_t *regions;
	int					 region_count;
	int					 region_capacity;
};

static char *atlas_copy_string(const char *text)
{
	if (text == NULL) return NULL;
	size_t length = strlen(text) + 1;
	char  *copy	  = malloc(length);
	if (copy != NULL) memcpy(copy, text, length);
	return copy;
}

static char *atlas_trim(char *text)
{
	while (*text == ' ' || *text == '\t') text++;
	char *end = text + strlen(text);
	while (end > text &&
		   (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
	return text;
}

static void atlas_skip_spaces(char **cursor)
{
	while (**cursor == ' ' || **cursor == '\t') (*cursor)++;
}

static int atlas_parse_word(char **cursor, char **out)
{
	atlas_skip_spaces(cursor);
	char *start = *cursor;
	while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t') (*cursor)++;
	if (*cursor == start) return -1;
	if (**cursor != '\0') {
		**cursor = '\0';
		(*cursor)++;
	}
	*out = start;
	return 0;
}

static int atlas_parse_int(char **cursor, int *out)
{
	atlas_skip_spaces(cursor);
	char *end	= NULL;
	long  value = strtol(*cursor, &end, 10);
	if (end == *cursor) return -1;
	*cursor = end;
	*out	= (int)value;
	return 0;
}

static void atlas_free_contents(bapi_atlas_t atlas)
{
	if (atlas == NULL) return;
	for (int i = 0; i < atlas->region_count; i++) free((void *)atlas->regions[i].name);
	free(atlas->regions);
	free(atlas->page_path);
	if (atlas->owns_texture && atlas->texture != NULL) bapi_texture_destroy(atlas->texture);
	free(atlas);
}

static int atlas_add_region(bapi_atlas_t atlas, const char *name, int x, int y, int w, int h)
{
	if (atlas->region_count >= atlas->region_capacity) {
		int new_capacity		   = atlas->region_capacity == 0 ? 8 : atlas->region_capacity * 2;
		bapi_atlas_region_t *grown = realloc(atlas->regions, (size_t)new_capacity * sizeof(*grown));
		if (grown == NULL) return -1;
		atlas->regions		   = grown;
		atlas->region_capacity = new_capacity;
	}

	char *name_copy = atlas_copy_string(name);
	if (name_copy == NULL) return -1;

	bapi_atlas_region_t *region = &atlas->regions[atlas->region_count++];
	region->name				= name_copy;
	region->x					= x;
	region->y					= y;
	region->w					= w;
	region->h					= h;
	return 0;
}

static int atlas_parse_manifest(bapi_atlas_t atlas, char *buffer)
{
	char *cursor	= buffer;
	int	  line_no	= 0;
	int	  have_head = 0;

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

		char *text = atlas_trim(line);
		if (*text == '\0' || *text == '#') continue;

		if (!have_head) {
			if (strcmp(text, "BE_ATLAS 1") != 0) {
				bapi_set_last_error("atlas manifest is missing the BE_ATLAS 1 header");
				return -1;
			}
			have_head = 1;
			continue;
		}

		if (strncmp(text, "page ", 5) == 0) {
			char *value = atlas_trim(text + 5);
			free(atlas->page_path);
			atlas->page_path = atlas_copy_string(value);
			if (atlas->page_path == NULL) return -1;
		} else if (strncmp(text, "size ", 5) == 0) {
			char *rest	= text + 5;
			int	  width = 0, height = 0;
			if (atlas_parse_int(&rest, &width) != 0 || atlas_parse_int(&rest, &height) != 0) {
				bapi_set_last_error("atlas manifest has an invalid size on line %d", line_no);
				return -1;
			}
			(void)width;
			(void)height;
		} else if (strncmp(text, "region ", 7) == 0) {
			char *rest = text + 7;
			char *name = NULL;
			int	  x = 0, y = 0, w = 0, h = 0;
			if (atlas_parse_word(&rest, &name) != 0 || atlas_parse_int(&rest, &x) != 0 ||
				atlas_parse_int(&rest, &y) != 0 || atlas_parse_int(&rest, &w) != 0 ||
				atlas_parse_int(&rest, &h) != 0) {
				bapi_set_last_error("atlas manifest has an invalid region on line %d", line_no);
				return -1;
			}
			if (w <= 0 || h <= 0 || x < 0 || y < 0) {
				bapi_set_last_error("atlas manifest has out-of-range region '%s'", name);
				return -1;
			}
			if (atlas_add_region(atlas, name, x, y, w, h) != 0) {
				bapi_set_last_error("out of memory while parsing atlas regions");
				return -1;
			}
		} else {
			bapi_set_last_error("atlas manifest has an unknown directive on line %d", line_no);
			return -1;
		}
	}

	if (!have_head) {
		bapi_set_last_error("atlas manifest is empty");
		return -1;
	}
	if (atlas->page_path == NULL) {
		bapi_set_last_error("atlas manifest does not name a page image");
		return -1;
	}
	return 0;
}

static char *atlas_resolve_page_path(const char *manifest_path, const char *page)
{
	if (page[0] == '/' || (page[0] != '\0' && page[1] == ':')) return atlas_copy_string(page);

	const char *last_forward  = strrchr(manifest_path, '/');
	const char *last_backward = strrchr(manifest_path, '\\');
	const char *slash		  = last_forward;
	if (last_backward != NULL && (slash == NULL || last_backward > slash)) slash = last_backward;
	if (slash == NULL) return atlas_copy_string(page);

	size_t directory_length = (size_t)(slash - manifest_path) + 1;
	size_t page_length		= strlen(page);
	char  *resolved			= malloc(directory_length + page_length + 1);
	if (resolved == NULL) return NULL;
	memcpy(resolved, manifest_path, directory_length);
	memcpy(resolved + directory_length, page, page_length + 1);
	return resolved;
}

static bapi_atlas_t atlas_parse_buffer(const void *data, size_t size)
{
	if (data == NULL || size == 0) {
		bapi_set_last_error("atlas manifest is empty");
		return NULL;
	}

	char *buffer = malloc(size + 1);
	if (buffer == NULL) {
		bapi_set_last_error("out of memory while reading atlas manifest");
		return NULL;
	}
	memcpy(buffer, data, size);
	buffer[size] = '\0';

	bapi_atlas_t atlas = calloc(1, sizeof(*atlas));
	if (atlas == NULL) {
		free(buffer);
		bapi_set_last_error("out of memory while creating atlas");
		return NULL;
	}

	if (atlas_parse_manifest(atlas, buffer) != 0) {
		free(buffer);
		atlas_free_contents(atlas);
		return NULL;
	}
	free(buffer);
	return atlas;
}

static bapi_atlas_t atlas_parse_file(const char *manifest_path)
{
	if (manifest_path == NULL || manifest_path[0] == '\0') {
		bapi_set_last_error("atlas manifest path is empty");
		return NULL;
	}

	size_t size = 0;
	uint8_t *bytes = bapi_file_read_alloc(manifest_path, &size);
	if (bytes == NULL) {
		bapi_set_last_error("atlas manifest could not be read: %s", manifest_path);
		return NULL;
	}

	bapi_atlas_t atlas = atlas_parse_buffer(bytes, size);
	free(bytes);
	if (atlas == NULL) return NULL;

	/* Keep the parsed page path relative to the manifest. */
	char *resolved = atlas_resolve_page_path(manifest_path, atlas->page_path);
	if (resolved == NULL) {
		atlas_free_contents(atlas);
		bapi_set_last_error("out of memory while resolving atlas page path");
		return NULL;
	}
	free(atlas->page_path);
	atlas->page_path = resolved;
	return atlas;
}

bapi_atlas_t bapi_atlas_load_with_texture(const char *manifest_path, bapi_texture_t page)
{
	if (page == NULL) {
		bapi_set_last_error("atlas page texture is NULL");
		return NULL;
	}
	bapi_atlas_t atlas = atlas_parse_file(manifest_path);
	if (atlas == NULL) return NULL;
	atlas->texture		= page;
	atlas->owns_texture = 0;
	return atlas;
}

bapi_atlas_t bapi_atlas_load_from_memory(const void *manifest, size_t size, bapi_texture_t page)
{
	if (page == NULL) {
		bapi_set_last_error("atlas page texture is NULL");
		return NULL;
	}
	bapi_atlas_t atlas = atlas_parse_buffer(manifest, size);
	if (atlas == NULL) return NULL;
	atlas->texture = page;
	atlas->owns_texture = 0;
	return atlas;
}

bapi_atlas_t bapi_atlas_load(const char *manifest_path)
{
	bapi_atlas_t atlas = atlas_parse_file(manifest_path);
	if (atlas == NULL) return NULL;

	bapi_texture_t texture = bapi_texture_from_file(atlas->page_path, NULL, NULL);
	if (texture == NULL) {
		bapi_set_last_error("atlas page image could not be loaded: %s", atlas->page_path);
		atlas_free_contents(atlas);
		return NULL;
	}
	atlas->texture		= texture;
	atlas->owns_texture = 1;
	return atlas;
}

void bapi_atlas_destroy(bapi_atlas_t atlas)
{
	atlas_free_contents(atlas);
}

bapi_texture_t bapi_atlas_get_texture(bapi_atlas_t atlas)
{
	return atlas != NULL ? atlas->texture : NULL;
}

int bapi_atlas_get_region_count(bapi_atlas_t atlas)
{
	return atlas != NULL ? atlas->region_count : 0;
}

const bapi_atlas_region_t *bapi_atlas_get_region(bapi_atlas_t atlas, int index)
{
	if (atlas == NULL || index < 0 || index >= atlas->region_count) return NULL;
	return &atlas->regions[index];
}

const bapi_atlas_region_t *bapi_atlas_find_region(bapi_atlas_t atlas, const char *name)
{
	if (atlas == NULL || name == NULL) return NULL;
	for (int i = 0; i < atlas->region_count; i++) {
		if (strcmp(atlas->regions[i].name, name) == 0) return &atlas->regions[i];
	}
	return NULL;
}

void bapi_atlas_draw(bapi_atlas_t atlas, const char *name, bapi_rect_t dst, int flip,
					 bapi_color_t tint)
{
	if (atlas == NULL || atlas->texture == NULL) return;
	const bapi_atlas_region_t *region = bapi_atlas_find_region(atlas, name);
	if (region == NULL) {
		bapi_set_last_error("atlas region not found: %s", name != NULL ? name : "(null)");
		return;
	}

	bapi_texture_draw_t params = bapi_texture_draw_default(dst);
	params.src =
		(bapi_rect_t){(float)region->x, (float)region->y, (float)region->w, (float)region->h};
	params.flip = flip;
	params.tint = tint;
	bapi_texture_draw(atlas->texture, &params);
}
