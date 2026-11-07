/*
 * Build-time RZip packer.
 *
 * Usage:
 *   bridgeengine_rz_pack -o OUT.rz FILE...
 *
 * The archive stores entries verbatim (RZip 0.1 has no compression); it is
 * intended to bundle atlas pages, manifests, fonts and other cooked assets
 * into one runtime pack readable by bapi_pack_open().
 */

#include "rz_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define rz_getcwd _getcwd
#define rz_chdir  _chdir
#else
#include <unistd.h>
#define rz_getcwd getcwd
#define rz_chdir  chdir
#endif

static void print_usage(const char *program)
{
	fprintf(stderr, "usage: %s [-C DIR] -o OUT.rz FILE...\n", program);
}

static char *absolute_path(const char *path)
{
	if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
		size_t length = strlen(path) + 1;
		char  *copy	  = malloc(length);
		if (copy != NULL) memcpy(copy, path, length);
		return copy;
	}
	char cwd[4096];
	if (rz_getcwd(cwd, sizeof(cwd)) == NULL) return NULL;
	size_t cwd_length  = strlen(cwd);
	size_t path_length = strlen(path);
	char  *result	   = malloc(cwd_length + path_length + 2);
	if (result != NULL) {
		snprintf(result, cwd_length + path_length + 2, "%s/%s", cwd, path);
	}
	return result;
}

int main(int argc, char **argv)
{
	const char *output	  = NULL;
	const char *directory = NULL;
	int			first	  = 1;

	while (first < argc) {
		if (strcmp(argv[first], "-o") == 0 && first + 1 < argc) {
			output = argv[first + 1];
			first += 2;
		} else if (strcmp(argv[first], "-C") == 0 && first + 1 < argc) {
			directory = argv[first + 1];
			first += 2;
		} else if (argv[first][0] == '-') {
			print_usage(argv[0]);
			return 1;
		} else {
			break;
		}
	}
	if (output == NULL || first >= argc) {
		print_usage(argv[0]);
		return 1;
	}

	char *output_absolute = absolute_path(output);
	if (output_absolute == NULL) {
		fprintf(stderr, "cannot resolve output path\n");
		return 1;
	}
	if (directory != NULL && rz_chdir(directory) != 0) {
		fprintf(stderr, "cannot enter directory %s\n", directory);
		free(output_absolute);
		return 1;
	}

	int		   file_count = argc - first;
	rz_file_t *files	  = calloc((size_t)file_count, sizeof(*files));
	if (files == NULL) return 1;

	rz_index_t index;
	memset(&index, 0, sizeof(index));
	for (int i = 0; i < file_count; i++) {
		files[i].filename = argv[first + i];
		if (rz_add_index(&files[i], &index) == NULL) {
			fprintf(stderr, "cannot add %s to archive index\n", argv[first + i]);
			free(files);
			return 1;
		}
	}

	int result = rz_create(output_absolute, &index, 0);
	if (result != 0) {
		fprintf(stderr, "cannot write archive %s\n", output);
	} else {
		printf("archive: %s (%d files)\n", output, file_count);
	}
	free(output_absolute);
	free(files);
	return result == 0 ? 0 : 1;
}
