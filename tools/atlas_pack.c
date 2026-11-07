/*
 * Build-time texture atlas packer.
 *
 * Usage:
 *   bridgeengine_atlas_pack -o OUT_DIR -n NAME [--max-width N]
 *                           [--format png|bmp] [--pack OUT.rz] IMAGE...
 *
 * Writes OUT_DIR/NAME.<png|bmp> and OUT_DIR/NAME.atlas. The manifest is a
 * line-oriented format consumed by bapi_atlas_load():
 *
 *   BE_ATLAS 1
 *   page NAME.png
 *   size 1024 512
 *   region player 0 0 32 32
 *   ...
 *
 * Images are trimmed? No: regions always describe the full source image.
 * A one-pixel transparent gutter separates entries to avoid bleeding.
 */

#include "rz_lib.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define atlas_chdir _chdir
#else
#include <unistd.h>
#define atlas_chdir chdir
#endif

#define ATLAS_PADDING  1
#define ATLAS_MAX_NAME 63

typedef struct {
	const char	*path;
	char		 name[ATLAS_MAX_NAME + 1];
	SDL_Surface *surface;
	int			 x;
	int			 y;
	int			 w;
	int			 h;
} atlas_image_t;

typedef struct {
	const char *output_dir;
	const char *name;
	const char *format;
	const char *pack_path;
	int			max_width;
} atlas_options_t;

static void print_usage(const char *program)
{
	fprintf(stderr,
			"usage: %s -o OUT_DIR -n NAME [--max-width N] [--format png|bmp] "
			"[--pack OUT.rz] IMAGE...\n",
			program);
}

static int parse_int(const char *text, int *out)
{
	char *end	= NULL;
	long  value = strtol(text, &end, 10);
	if (end == text || *end != '\0' || value <= 0 || value > 65536) return -1;
	*out = (int)value;
	return 0;
}

static int parse_options(int argc, char **argv, atlas_options_t *options)
{
	options->output_dir = ".";
	options->name		= "atlas";
	options->format		= "png";
	options->pack_path	= NULL;
	options->max_width	= 2048;

	int index = 1;
	while (index < argc) {
		const char *argument = argv[index];
		if (strcmp(argument, "-o") == 0 && index + 1 < argc) {
			options->output_dir = argv[++index];
		} else if (strcmp(argument, "-n") == 0 && index + 1 < argc) {
			options->name = argv[++index];
		} else if (strcmp(argument, "--max-width") == 0 && index + 1 < argc) {
			if (parse_int(argv[++index], &options->max_width) != 0) {
				fprintf(stderr, "invalid --max-width value\n");
				return -1;
			}
		} else if (strcmp(argument, "--format") == 0 && index + 1 < argc) {
			options->format = argv[++index];
		} else if (strcmp(argument, "--pack") == 0 && index + 1 < argc) {
			options->pack_path = argv[++index];
		} else if (argument[0] == '-') {
			fprintf(stderr, "unknown option: %s\n", argument);
			return -1;
		} else {
			break;
		}
		index++;
	}

	if (index >= argc) return -1;
	if (strcmp(options->format, "png") != 0 && strcmp(options->format, "bmp") != 0) {
		fprintf(stderr, "--format must be png or bmp\n");
		return -1;
	}
	return index;
}

static const char *base_name(const char *path)
{
	const char *forward	 = strrchr(path, '/');
	const char *backward = strrchr(path, '\\');
	const char *base	 = forward;
	if (backward != NULL && (base == NULL || backward > base)) base = backward;
	return base != NULL ? base + 1 : path;
}

static void make_region_name(const char *path, char *out, size_t capacity)
{
	const char *base   = base_name(path);
	size_t		length = strlen(base);
	const char *dot	   = strrchr(base, '.');
	if (dot != NULL) length = (size_t)(dot - base);
	if (length >= capacity) length = capacity - 1;

	size_t write = 0;
	for (size_t i = 0; i < length && write + 1 < capacity; i++) {
		unsigned char c = (unsigned char)base[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '_' || c == '-') {
			out[write++] = (char)c;
		} else {
			out[write++] = '_';
		}
	}
	if (write == 0) {
		snprintf(out, capacity, "region");
		write = strlen(out);
	}
	out[write] = '\0';
}

static void make_unique_name(char *name, size_t capacity, char names[][ATLAS_MAX_NAME + 1],
							 int count)
{
	for (int attempt = 2; attempt < 10000; attempt++) {
		int conflict = 0;
		for (int i = 0; i < count; i++) {
			if (strcmp(names[i], name) == 0) {
				conflict = 1;
				break;
			}
		}
		if (!conflict) return;
		char suffix[16];
		snprintf(suffix, sizeof(suffix), "_%d", attempt);
		size_t base_length	 = strlen(name);
		size_t suffix_length = strlen(suffix);
		if (base_length + suffix_length >= capacity) base_length = capacity - suffix_length - 1;
		memcpy(name + base_length, suffix, suffix_length + 1);
	}
}

static int compare_height_desc(const void *left, const void *right)
{
	const atlas_image_t *a = (const atlas_image_t *)left;
	const atlas_image_t *b = (const atlas_image_t *)right;
	if (a->h != b->h) return b->h - a->h;
	return strcmp(a->path, b->path);
}

static int next_power_of_two(int value)
{
	int result = 1;
	while (result < value && result < 65536) result <<= 1;
	return result;
}

static int load_image(const char *path, atlas_image_t *image)
{
	SDL_Surface *loaded = IMG_Load(path);
	if (loaded == NULL) {
		fprintf(stderr, "cannot load image %s: %s\n", path, SDL_GetError());
		return -1;
	}
	SDL_Surface *converted = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888);
	SDL_DestroySurface(loaded);
	if (converted == NULL) {
		fprintf(stderr, "cannot convert image %s: %s\n", path, SDL_GetError());
		return -1;
	}
	image->path	   = path;
	image->surface = converted;
	image->w	   = converted->w;
	image->h	   = converted->h;
	return 0;
}

static int build_manifest(const atlas_options_t *options, const char *page_file,
						  const atlas_image_t *images, int image_count, int page_width,
						  int page_height, const char *manifest_path)
{
	FILE *file = fopen(manifest_path, "wb");
	if (file == NULL) {
		fprintf(stderr, "cannot write manifest %s\n", manifest_path);
		return -1;
	}
	fprintf(file, "BE_ATLAS 1\n");
	fprintf(file, "page %s\n", page_file);
	fprintf(file, "size %d %d\n", page_width, page_height);
	for (int i = 0; i < image_count; i++) {
		fprintf(file, "region %s %d %d %d %d\n", images[i].name, images[i].x, images[i].y,
				images[i].w, images[i].h);
	}
	if (fclose(file) != 0) {
		fprintf(stderr, "cannot finish manifest %s\n", manifest_path);
		return -1;
	}
	return 0;
}

static char *absolute_path(const char *path)
{
	if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
		size_t length = strlen(path) + 1;
		char  *copy	  = malloc(length);
		if (copy != NULL) memcpy(copy, path, length);
		return copy;
	}
	char *cwd = SDL_GetCurrentDirectory();
	if (cwd == NULL) return NULL;
	size_t cwd_length  = strlen(cwd);
	size_t path_length = strlen(path);
	char  *result	   = malloc(cwd_length + path_length + 2);
	if (result != NULL) {
		snprintf(result, cwd_length + path_length + 2, "%s/%s", cwd, path);
	}
	SDL_free(cwd);
	return result;
}

static int pack_archive(const char *output_dir, const char *pack_path, const char *page_file,
						const char *manifest_file)
{
	char *pack_absolute = absolute_path(pack_path);
	char *old_directory = SDL_GetCurrentDirectory();
	if (pack_absolute == NULL || old_directory == NULL) {
		free(pack_absolute);
		SDL_free(old_directory);
		fprintf(stderr, "cannot resolve archive paths\n");
		return -1;
	}
	if (atlas_chdir(output_dir) != 0) {
		fprintf(stderr, "cannot enter output directory %s\n", output_dir);
		free(pack_absolute);
		SDL_free(old_directory);
		return -1;
	}

	rz_index_t index;
	memset(&index, 0, sizeof(index));
	rz_file_t page;
	rz_file_t manifest;
	memset(&page, 0, sizeof(page));
	memset(&manifest, 0, sizeof(manifest));
	page.filename	  = (char *)page_file;
	manifest.filename = (char *)manifest_file;
	int result		  = 0;
	if (rz_add_index(&page, &index) == NULL || rz_add_index(&manifest, &index) == NULL) {
		fprintf(stderr, "cannot build archive index\n");
		result = -1;
	} else if (rz_create(pack_absolute, &index, 0) != 0) {
		fprintf(stderr, "cannot write archive %s\n", pack_path);
		result = -1;
	}

	atlas_chdir(old_directory);
	SDL_free(old_directory);
	free(pack_absolute);
	return result;
}

int main(int argc, char **argv)
{
	SDL_SetAppMetadata("BridgeEngine atlas packer", "1.0", "com.bridgeengine.atlas");
	if (!SDL_Init(0)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	atlas_options_t options;
	int				first_image = parse_options(argc, argv, &options);
	if (first_image < 0) {
		print_usage(argv[0]);
		SDL_Quit();
		return 1;
	}

	int			   image_count = argc - first_image;
	atlas_image_t *images	   = calloc((size_t)image_count, sizeof(*images));
	if (images == NULL) {
		SDL_Quit();
		return 1;
	}

	char (*names)[ATLAS_MAX_NAME + 1] = calloc((size_t)image_count, ATLAS_MAX_NAME + 1);
	if (names == NULL) {
		free(images);
		SDL_Quit();
		return 1;
	}

	int result = 0;
	for (int i = 0; i < image_count; i++) {
		if (load_image(argv[first_image + i], &images[i]) != 0) {
			result = 1;
			break;
		}
		make_region_name(images[i].path, images[i].name, sizeof(images[i].name));
		make_unique_name(images[i].name, sizeof(images[i].name), names, i);
		memcpy(names[i], images[i].name, sizeof(names[i]));
	}
	if (result == 0) {
		qsort(images, (size_t)image_count, sizeof(*images), compare_height_desc);

		int cursor_x   = 0;
		int cursor_y   = 0;
		int row_height = 0;
		int used_width = 0;
		for (int i = 0; i < image_count; i++) {
			int cell_width	= images[i].w + 2 * ATLAS_PADDING;
			int cell_height = images[i].h + 2 * ATLAS_PADDING;
			if (cell_width > options.max_width) {
				fprintf(stderr, "image %s is wider than --max-width\n", images[i].path);
				result = 1;
				break;
			}
			if (cursor_x > 0 && cursor_x + cell_width > options.max_width) {
				cursor_x = 0;
				cursor_y += row_height;
				row_height = 0;
			}
			images[i].x = cursor_x + ATLAS_PADDING;
			images[i].y = cursor_y + ATLAS_PADDING;
			cursor_x += cell_width;
			if (cursor_x > used_width) used_width = cursor_x;
			if (cell_height > row_height) row_height = cell_height;
		}

		if (result == 0) {
			int page_width	= next_power_of_two(used_width);
			int page_height = next_power_of_two(cursor_y + row_height);
			if (page_width < 1) page_width = 1;
			if (page_height < 1) page_height = 1;

			SDL_Surface *page =
				SDL_CreateSurface(page_width, page_height, SDL_PIXELFORMAT_ARGB8888);
			if (page == NULL) {
				fprintf(stderr, "cannot create atlas page: %s\n", SDL_GetError());
				result = 1;
			} else {
				SDL_FillSurfaceRect(page, NULL, 0);
				for (int i = 0; i < image_count; i++) {
					SDL_Rect destination = {images[i].x, images[i].y, images[i].w, images[i].h};
					if (!SDL_BlitSurface(images[i].surface, NULL, page, &destination)) {
						fprintf(stderr, "cannot blit %s: %s\n", images[i].path, SDL_GetError());
						result = 1;
						break;
					}
				}

				if (result == 0) {
					char page_file[512];
					char manifest_file[512];
					char page_path[1024];
					char manifest_path[1024];
					snprintf(page_file, sizeof(page_file), "%s.%s", options.name, options.format);
					snprintf(manifest_file, sizeof(manifest_file), "%s.atlas", options.name);
					snprintf(page_path, sizeof(page_path), "%s/%s", options.output_dir, page_file);
					snprintf(manifest_path, sizeof(manifest_path), "%s/%s", options.output_dir,
							 manifest_file);

					SDL_CreateDirectory(options.output_dir);

					int saved = strcmp(options.format, "bmp") == 0 ? SDL_SaveBMP(page, page_path)
																   : IMG_SavePNG(page, page_path);
					if (!saved) {
						fprintf(stderr, "cannot save atlas page %s: %s\n", page_path,
								SDL_GetError());
						result = 1;
					} else if (build_manifest(&options, page_file, images, image_count, page_width,
											  page_height, manifest_path) != 0) {
						result = 1;
					} else if (options.pack_path != NULL &&
							   pack_archive(options.output_dir, options.pack_path, page_file,
											manifest_file) != 0) {
						result = 1;
					} else {
						printf("atlas: %s (%dx%d, %d regions)\n", manifest_path, page_width,
							   page_height, image_count);
					}
				}
				SDL_DestroySurface(page);
			}
		}
	}

	for (int i = 0; i < image_count; i++) {
		if (images[i].surface != NULL) SDL_DestroySurface(images[i].surface);
	}
	free(names);
	free(images);
	SDL_Quit();
	return result;
}
