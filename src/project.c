#include "BridgeEngine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bapi_project_internal {
	char  *name;
	char  *engine;
	char **documents;
	int	   document_count;
};

// Strip leading/trailing spaces and tabs from value.
static char *trim(char *value)
{
	char *start = value;
	char *end;

	while (*start == ' ' || *start == '\t') start++;
	end = start + strlen(start);
	while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
	*end = '\0';
	return start;
}

// Portable strdup (not a C standard function; avoids feature-test macros).
static char *copy_str(const char *value)
{
	if (!value) return NULL;
	size_t len	= strlen(value);
	char  *copy = malloc(len + 1);
	if (copy) memcpy(copy, value, len + 1);
	return copy;
}

// Reject absolute paths: POSIX root, Windows drive letters, and UNC prefixes.
// .bep document entries are project-relative by contract.
static int is_absolute_document_path(const char *path)
{
	if (path[0] == '/' || path[0] == '\\') return 1;
	if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
		path[1] == ':')
		return 1;
	return 0;
}

// Normalize a document path for storage: forward slashes only, no leading "./",
// no empty or "." segments, and no ".." segments (they would escape the project
// root). Returns a freshly allocated string, or NULL for empty / invalid input
// or on allocation failure. The caller owns the result.
static char *normalize_document_path(const char *path)
{
	if (!path) return NULL;

	size_t length = strlen(path);
	char  *work	  = malloc(length + 1);
	if (!work) return NULL;
	memcpy(work, path, length + 1);

	char  *trimmed	   = trim(work);
	size_t trimmed_len = strlen(trimmed);
	memmove(work, trimmed, trimmed_len + 1);

	if (trimmed_len == 0 || is_absolute_document_path(work)) {
		free(work);
		return NULL;
	}

	for (size_t i = 0; i < trimmed_len; i++)
		if (work[i] == '\\') work[i] = '/';

	char *out = malloc(trimmed_len + 1);
	if (!out) {
		free(work);
		return NULL;
	}
	size_t		out_len = 0;
	const char *cursor	= work;
	int			first	= 1;
	while (*cursor) {
		const char *segment = cursor;
		while (*cursor && *cursor != '/') cursor++;
		size_t segment_len = (size_t)(cursor - segment);
		if (segment_len == 0 || (segment_len == 1 && segment[0] == '.')) {
			// Drop empty and "." segments; this also strips leading "./".
		} else if (segment_len == 2 && segment[0] == '.' && segment[1] == '.') {
			free(out);
			free(work);
			return NULL;
		} else {
			if (!first) out[out_len++] = '/';
			memcpy(out + out_len, segment, segment_len);
			out_len += segment_len;
			first = 0;
		}
		if (*cursor == '/') cursor++;
	}
	free(work);

	if (out_len == 0) {
		free(out);
		return NULL;
	}
	out[out_len] = '\0';
	return out;
}

// Compare a stored document path with a caller-supplied one after normalizing
// both. Returns 1 when they refer to the same normalized path.
static int document_paths_equal(const char *stored, const char *normalized)
{
	if (!stored || !normalized) return 0;
	char *stored_normalized = normalize_document_path(stored);
	int	  equal				= stored_normalized && strcmp(stored_normalized, normalized) == 0;
	free(stored_normalized);
	return equal;
}

// Parse .bep contents held in a NUL-terminated buffer. Returns a project or
// NULL. `buf` is borrowed (not freed); document strings are duplicated.
static bapi_project_t parse_bep_buffer(const char *buf)
{
	bapi_project_t project = (bapi_project_t)calloc(1, sizeof(struct bapi_project_internal));
	if (!project) return NULL;

	const char *cursor		 = buf;
	int			in_documents = 0;
	while (cursor && *cursor) {
		const char *line_end = strchr(cursor, '\n');
		size_t		length	 = line_end ? (size_t)(line_end - cursor) : strlen(cursor);
		if (length > 0 && cursor[length - 1] == '\r') length--;

		char *line = malloc(length + 1);
		if (!line) {
			bapi_project_destroy(project);
			return NULL;
		}
		memcpy(line, cursor, length);
		line[length] = '\0';
		char *value	 = trim(line);
		if (*value && *value != '#' && *value != ';') {
			if (value[0] == '[' && value[strlen(value) - 1] == ']') {
				in_documents = (strcmp(value, "[documents]") == 0);
			} else if (in_documents) {
				char **grown = (char **)realloc(
					project->documents, (size_t)(project->document_count + 1) * sizeof(char *));
				if (!grown) {
					free(line);
					bapi_project_destroy(project);
					return NULL;
				}
				project->documents							= grown;
				project->documents[project->document_count] = copy_str(value);
				if (!project->documents[project->document_count]) {
					free(line);
					bapi_project_destroy(project);
					return NULL;
				}
				project->document_count++;
			} else {
				char *equals = strchr(value, '=');
				if (equals) {
					*equals	   = '\0';
					char *key  = trim(value);
					char *data = trim(equals + 1);
					if (strcmp(key, "name") == 0) {
						free(project->name);
						project->name = copy_str(data);
					} else if (strcmp(key, "engine") == 0) {
						free(project->engine);
						project->engine = copy_str(data);
					}
				}
			}
		}
		free(line);
		cursor = line_end ? line_end + 1 : NULL;
	}
	return project;
}

bapi_project_t bapi_project_load_from_bep(const char *filepath)
{
	if (!filepath || !filepath[0]) return NULL;

	FILE *file = fopen(filepath, "rb");
	if (!file) return NULL;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size < 0) {
		fclose(file);
		return NULL;
	}
	char *buf = malloc((size_t)size + 1);
	if (!buf) {
		fclose(file);
		return NULL;
	}
	if (size > 0 && fread(buf, 1, (size_t)size, file) != (size_t)size) {
		free(buf);
		fclose(file);
		return NULL;
	}
	buf[size] = '\0';
	fclose(file);

	bapi_project_t project = parse_bep_buffer(buf);
	free(buf);
	return project;
}

void bapi_project_destroy(bapi_project_t project)
{
	if (!project) return;
	free(project->name);
	free(project->engine);
	for (int i = 0; i < project->document_count; i++) free(project->documents[i]);
	free(project->documents);
	memset(project, 0, sizeof(struct bapi_project_internal));
	free(project);
}

const char *bapi_project_get_name(bapi_project_t project)
{
	return project ? project->name : NULL;
}

const char *bapi_project_get_engine_dir(bapi_project_t project)
{
	return project ? project->engine : NULL;
}

int bapi_project_get_document_count(bapi_project_t project)
{
	return project ? project->document_count : 0;
}

const char *bapi_project_get_document_path(bapi_project_t project, int index)
{
	if (!project || index < 0 || index >= project->document_count) return NULL;
	return project->documents[index];
}

bapi_project_t bapi_project_create(const char *name, const char *engine_dir)
{
	if (!name || !engine_dir) return NULL;

	bapi_project_t project = (bapi_project_t)calloc(1, sizeof(struct bapi_project_internal));
	if (!project) return NULL;

	project->name	= copy_str(name);
	project->engine = copy_str(engine_dir);
	if (!project->name || !project->engine) {
		bapi_project_destroy(project);
		return NULL;
	}
	return project;
}

int bapi_project_save(bapi_project_t project, const char *filepath)
{
	if (!project || !filepath || !filepath[0]) return -1;

#ifdef USE_BACKEND_XJ380
	// The XJ380 sysroot has no remove/rename declarations; write in place.
	FILE *file = fopen(filepath, "wb");
	if (!file) return -1;
#else
	// Write to "<filepath>.tmp" first, then rename over the destination so a
	// partially written project file is never observed.
	size_t path_len	 = strlen(filepath);
	char  *temp_path = malloc(path_len + 5);
	if (!temp_path) return -1;
	memcpy(temp_path, filepath, path_len);
	memcpy(temp_path + path_len, ".tmp", 5);

	FILE *file = fopen(temp_path, "wb");
	if (!file) {
		free(temp_path);
		return -1;
	}
#endif

	int ok = 1;
	if (fprintf(file, "# BridgeEngine Project\n") < 0) ok = 0;
	if (ok && fprintf(file, "name = %s\n", project->name ? project->name : "") < 0) ok = 0;
	if (ok && fprintf(file, "engine = %s\n", project->engine ? project->engine : "") < 0) ok = 0;
	if (ok && fprintf(file, "\n[documents]\n") < 0) ok = 0;
	for (int i = 0; ok && i < project->document_count; i++) {
		if (fprintf(file, "%s\n", project->documents[i]) < 0) ok = 0;
	}
	if (ok && fflush(file) != 0) ok = 0;
	if (fclose(file) != 0) ok = 0;

	if (!ok) {
#ifndef USE_BACKEND_XJ380
		remove(temp_path);
		free(temp_path);
#endif
		return -1;
	}

#ifndef USE_BACKEND_XJ380
	if (rename(temp_path, filepath) != 0) {
		// Some platforms refuse to rename over an existing file; fall back to
		// remove + rename rather than leaving the stale project file in place.
		remove(filepath);
		if (rename(temp_path, filepath) != 0) {
			remove(temp_path);
			free(temp_path);
			return -1;
		}
	}
	free(temp_path);
#endif
	return 0;
}

int bapi_project_add_document(bapi_project_t project, const char *path)
{
	if (!project || !path) return -1;

	char *normalized = normalize_document_path(path);
	if (!normalized) return -1;

	// Adding a document that is already listed is a successful no-op.
	for (int i = 0; i < project->document_count; i++) {
		if (document_paths_equal(project->documents[i], normalized)) {
			free(normalized);
			return 0;
		}
	}

	char **grown = (char **)realloc(project->documents,
									(size_t)(project->document_count + 1) * sizeof(char *));
	if (!grown) {
		free(normalized);
		return -1;
	}
	project->documents							= grown;
	project->documents[project->document_count] = normalized;
	project->document_count++;
	return 0;
}

int bapi_project_remove_document(bapi_project_t project, const char *path)
{
	if (!project || !path) return -1;

	char *normalized = normalize_document_path(path);
	if (!normalized) return -1;

	int found = -1;
	for (int i = 0; i < project->document_count; i++) {
		if (document_paths_equal(project->documents[i], normalized)) {
			found = i;
			break;
		}
	}
	free(normalized);
	if (found < 0) return -1;

	free(project->documents[found]);
	for (int i = found; i + 1 < project->document_count; i++)
		project->documents[i] = project->documents[i + 1];
	project->document_count--;
	project->documents[project->document_count] = NULL;
	return 0;
}

int bapi_project_set_engine(bapi_project_t project, const char *engine_dir)
{
	if (!project || !engine_dir) return -1;

	char *copy = copy_str(engine_dir);
	if (!copy) return -1;
	free(project->engine);
	project->engine = copy;
	return 0;
}
