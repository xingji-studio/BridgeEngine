#include "BridgeEngine.h"
#include "internal/bapi_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSET_MANIFEST_LINE_MAX 1024

struct bapi_asset_manifest_internal {
	char **entries;
	int	   count;
	int	   capacity;
	char  *first_missing;
};

static char *duplicate_text(const char *text)
{
	size_t length = strlen(text) + 1;
	char  *copy	  = (char *)malloc(length);
	if (copy != NULL) memcpy(copy, text, length);
	return copy;
}

static char *trim_line(char *line)
{
	while (*line != '\0' && isspace((unsigned char)*line)) line++;
	char *end = line + strlen(line);
	while (end > line && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return line;
}

static void manifest_free(bapi_asset_manifest_t manifest)
{
	if (manifest == NULL) return;
	for (int i = 0; i < manifest->count; i++) free(manifest->entries[i]);
	free(manifest->entries);
	free(manifest->first_missing);
	free(manifest);
}

static int manifest_add(bapi_asset_manifest_t manifest, const char *path)
{
	if (manifest->count == manifest->capacity) {
		int	   capacity = manifest->capacity > 0 ? manifest->capacity * 2 : 8;
		char **entries	= (char **)realloc(manifest->entries, (size_t)capacity * sizeof(*entries));
		if (entries == NULL) return -1;
		manifest->entries  = entries;
		manifest->capacity = capacity;
	}
	char *copy = duplicate_text(path);
	if (copy == NULL) return -1;
	manifest->entries[manifest->count++] = copy;
	return 0;
}

static int path_is_absolute(const char *path)
{
#ifdef _WIN32
	if (path[0] == '/' || path[0] == '\\') return 1;
	if (isalpha((unsigned char)path[0]) && path[1] == ':') return 1;
#else
	if (path[0] == '/') return 1;
#endif
	return 0;
}

bapi_asset_manifest_t bapi_asset_manifest_load(const char *filepath)
{
	if (filepath == NULL || filepath[0] == '\0') {
		bapi_set_last_error("asset manifest path is NULL or empty");
		return NULL;
	}

	FILE *file = fopen(filepath, "rb");
	if (file == NULL) {
		bapi_set_last_error("cannot open asset manifest '%s'", filepath);
		return NULL;
	}

	bapi_asset_manifest_t manifest = (bapi_asset_manifest_t)calloc(1, sizeof(*manifest));
	if (manifest == NULL) {
		fclose(file);
		bapi_set_last_error("out of memory while loading asset manifest '%s'", filepath);
		return NULL;
	}

	char line[ASSET_MANIFEST_LINE_MAX];
	int	 line_number = 0;
	int	 header_seen = 0;

	while (fgets(line, (int)sizeof(line), file) != NULL) {
		line_number++;
		if (strchr(line, '\n') == NULL && !feof(file)) {
			bapi_set_last_error("asset manifest '%s' line %d is too long", filepath, line_number);
			goto fail;
		}

		char *text = trim_line(line);
		char *hash = strchr(text, '#');
		if (hash != NULL) {
			*hash = '\0';
			text  = trim_line(text);
		}
		if (text[0] == '\0') continue;

		if (!header_seen) {
			char keyword[32];
			int	 version = 0;
			if (sscanf(text, "%31s %d", keyword, &version) != 2 ||
				strcmp(keyword, "BASSETS") != 0) {
				bapi_set_last_error(
					"asset manifest '%s' line %d: expected 'BASSETS <version>' header", filepath,
					line_number);
				goto fail;
			}
			if (version < 1 || version > BAPI_ASSET_MANIFEST_VERSION) {
				bapi_set_last_error(
					"asset manifest '%s' line %d: unsupported version %d (expected 1..%d)",
					filepath, line_number, version, BAPI_ASSET_MANIFEST_VERSION);
				goto fail;
			}
			header_seen = 1;
			continue;
		}

		char *separator = text;
		while (*separator != '\0' && !isspace((unsigned char)*separator)) separator++;
		char saved	= *separator;
		*separator	= '\0';
		char *key	= text;
		char *value = separator;
		if (saved != '\0') value = separator + 1;
		while (*value != '\0' && isspace((unsigned char)*value)) value++;

		if (strcmp(key, "asset") == 0) {
			char *path = trim_line(value);
			if (path[0] == '\0') {
				bapi_set_last_error("asset manifest '%s' line %d: asset path is empty", filepath,
									line_number);
				goto fail;
			}
			if (manifest_add(manifest, path) != 0) {
				bapi_set_last_error("out of memory while loading asset manifest '%s'", filepath);
				goto fail;
			}
		}
	}

	if (!header_seen) {
		bapi_set_last_error("asset manifest '%s' is empty or missing the BASSETS header", filepath);
		goto fail;
	}

	fclose(file);
	return manifest;

fail:
	fclose(file);
	manifest_free(manifest);
	return NULL;
}

void bapi_asset_manifest_destroy(bapi_asset_manifest_t manifest)
{
	manifest_free(manifest);
}

int bapi_asset_manifest_count(bapi_asset_manifest_t manifest)
{
	if (manifest == NULL) {
		bapi_set_last_error("asset manifest is NULL");
		return 0;
	}
	return manifest->count;
}

const char *bapi_asset_manifest_get(bapi_asset_manifest_t manifest, int index)
{
	if (manifest == NULL) {
		bapi_set_last_error("asset manifest is NULL");
		return NULL;
	}
	if (index < 0 || index >= manifest->count) {
		bapi_set_last_error("asset manifest index %d is out of range (count %d)", index,
							manifest->count);
		return NULL;
	}
	return manifest->entries[index];
}

int bapi_asset_manifest_verify(bapi_asset_manifest_t manifest, const char *root, int *checked)
{
	if (checked != NULL) *checked = 0;
	if (manifest == NULL) {
		bapi_set_last_error("asset manifest is NULL");
		return -1;
	}

	free(manifest->first_missing);
	manifest->first_missing = NULL;
	if (checked != NULL) *checked = manifest->count;

	const char *base	= root != NULL ? root : "";
	int			missing = 0;
	for (int i = 0; i < manifest->count; i++) {
		const char *entry = manifest->entries[i];
		char		path[4096];
		int			length = 0;

		if (base[0] == '\0' || path_is_absolute(entry)) {
			length = snprintf(path, sizeof(path), "%s", entry);
		} else {
			size_t base_length = strlen(base);
			if (base[base_length - 1] == '/' || base[base_length - 1] == '\\')
				length = snprintf(path, sizeof(path), "%s%s", base, entry);
			else
				length = snprintf(path, sizeof(path), "%s/%s", base, entry);
		}

		if (length < 0 || (size_t)length >= sizeof(path)) {
			bapi_set_last_error("asset path is too long: '%s'", entry);
			missing++;
			if (manifest->first_missing == NULL) {
				manifest->first_missing = duplicate_text(entry);
				if (manifest->first_missing == NULL)
					bapi_set_last_error("out of memory while recording missing asset '%s'", entry);
			}
			continue;
		}

		FILE *file = fopen(path, "rb");
		if (file == NULL) {
			missing++;
			if (manifest->first_missing == NULL) {
				manifest->first_missing = duplicate_text(entry);
				if (manifest->first_missing == NULL)
					bapi_set_last_error("out of memory while recording missing asset '%s'", entry);
			}
		} else {
			fclose(file);
		}
	}

	return missing;
}

const char *bapi_asset_manifest_first_missing(bapi_asset_manifest_t manifest)
{
	if (manifest == NULL) {
		bapi_set_last_error("asset manifest is NULL");
		return NULL;
	}
	return manifest->first_missing;
}
