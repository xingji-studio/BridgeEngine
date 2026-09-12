#include "BridgeEngine.h"
#include "rz_lib.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Resource pack reader built on the vendored RZip library (thirdparty/rzip).
 *
 * Safety notes:
 * - rz_read_file() (rz_lib.c) does not validate offset/count against the
 *   entry's comp_size, so every read here is clamped to the entry's stored
 *   size before calling into the library.
 * - RZip keeps one shared FILE* per archive and a process-global registry of
 *   open archives, so this API is single-threaded by contract: one handle
 *   per pack, handles are not copied.
 * - The library is store-only today (comp_size == dcom_size); when real
 *   compression lands, read_file/read_file_alloc keep returning the logical
 *   (decompressed) bytes and this wrapper must switch to the decompressor.
 */

typedef struct {
	rz_file_t *node;
	int		   ordinal;
} pack_name_entry_t;

struct bapi_pack_internal {
	rz_t rz;
	rz_file_t		 **entries;
	pack_name_entry_t *names;
	size_t			   entry_count;
};

/* rz_read_file()'s count parameter is an int; read large entries in chunks. */
#define PACK_READ_CHUNK 65536u

static int pack_compare_names(const void *left, const void *right)
{
	const pack_name_entry_t *a = left, *b = right;
	int						 order = strcmp(rz_get_file_name(a->node), rz_get_file_name(b->node));
	if (order != 0) return order;
	/* Sorting duplicate names by archive position preserves first-match lookup. */
	return (a->ordinal > b->ordinal) - (a->ordinal < b->ordinal);
}

static void pack_build_index(bapi_pack_t pack)
{
	if (!pack->rz.index) return;
	size_t count = 0;
	for (rz_file_t *node = pack->rz.index->head; node; node = node->next) {
		if (count == INT_MAX) return;
		count++;
	}
	if (count == 0 || count > SIZE_MAX / sizeof(*pack->entries) ||
		count > SIZE_MAX / sizeof(*pack->names))
		return;
	rz_file_t **entries = malloc(count * sizeof(*entries));
	if (!entries) return;
	pack_name_entry_t *names = malloc(count * sizeof(*names));
	if (!names) {
		free(entries);
		return;
	}
	size_t i = 0;
	for (rz_file_t *node = pack->rz.index->head; node; node = node->next, i++) {
		entries[i] = node;
		names[i]   = (pack_name_entry_t){node, (int)i};
	}
	qsort(names, count, sizeof(*names), pack_compare_names);
	pack->entries	  = entries;
	pack->names		  = names;
	pack->entry_count = count;
}

static int pack_find_index(bapi_pack_t pack, const char *name)
{
	if (!pack || !pack->rz.index || !name) return -1;
	if (pack->names) {
		size_t low = 0, high = pack->entry_count;
		while (low < high) {
			size_t middle = low + (high - low) / 2;
			if (strcmp(rz_get_file_name(pack->names[middle].node), name) < 0) {
				low = middle + 1;
			} else {
				high = middle;
			}
		}
		if (low < pack->entry_count && strcmp(rz_get_file_name(pack->names[low].node), name) == 0) {
			return pack->names[low].ordinal;
		}
		return -1;
	}
	/* Index allocation is optional: low-memory callers retain the old behavior. */
	int index = 0;
	for (rz_file_t *node = pack->rz.index->head; node; node = node->next) {
		if (strcmp(rz_get_file_name(node), name) == 0) return index;
		if (index == INT_MAX) break;
		index++;
	}
	return -1;
}

static rz_file_t *pack_find_node(bapi_pack_t pack, const char *name)
{
	if (!pack || !pack->rz.header || !pack->rz.index || !name) return NULL;
	if (pack->entries) {
		int index = pack_find_index(pack, name);
		return index < 0 ? NULL : pack->entries[index];
	}
	for (rz_file_t *node = pack->rz.index->head; node != NULL; node = node->next) {
		if (strcmp(rz_get_file_name(node), name) == 0) {
			return node;
		}
	}
	return NULL;
}

/* Copy up to `capacity` bytes of the entry's stored data starting at `offset`
 * into `dest` in bounded chunks; returns the number of bytes copied (partial
 * on I/O error). */
static size_t pack_copy_file(rz_file_t *node, uint64_t offset, uint8_t *dest, size_t capacity)
{
	uint64_t total = rz_get_file_comp_size(node);
	uint64_t want	= total < (uint64_t)capacity ? total : (uint64_t)capacity;
	size_t	 done	= 0;

	while (done < want) {
		/* rz_read_file() takes a `long` offset; on LLP64 platforms entries
		 * past LONG_MAX bytes are not addressable through it, so stop there. */
		if (offset > (uint64_t)LONG_MAX || done > (uint64_t)LONG_MAX - offset) break;

		size_t chunk = (size_t)(want - done);
		if (chunk > PACK_READ_CHUNK) chunk = PACK_READ_CHUNK;

		uint8_t *part = rz_read_file(node, (long)(offset + done), (int)chunk);
		if (!part) break;
		memcpy(dest + done, part, chunk);
		free(part);
		done += chunk;
	}

	return done;
}

bapi_pack_t bapi_pack_open(const char *path)
{
	if (!path || !path[0]) return NULL;

	rz_t rz = rz_open(path);
	if (rz.header == NULL) {
		return NULL;
	}

	bapi_pack_t pack = calloc(1, sizeof(*pack));
	if (!pack) {
		rz_close(&rz);
		return NULL;
	}
	pack->rz = rz;
	pack_build_index(pack);
	return pack;
}

void bapi_pack_close(bapi_pack_t pack)
{
	if (!pack) return;
	free(pack->names);
	free(pack->entries);
	rz_close(&pack->rz);
	free(pack);
}

int bapi_pack_file_count(bapi_pack_t pack)
{
	if (!pack || !pack->rz.header) return -1;
	uint32_t count = pack->rz.header->file_count;
	return count > (uint32_t)INT_MAX ? INT_MAX : (int)count;
}

const char *bapi_pack_file_name(bapi_pack_t pack, int index)
{
	if (!pack || !pack->rz.index || index < 0) return NULL;
	if (pack->entries) {
		return (size_t)index < pack->entry_count ? rz_get_file_name(pack->entries[index]) : NULL;
	}
	rz_file_t *node = pack->rz.index->head;
	for (int i = 0; node != NULL; node = node->next, i++) {
		if (i == index) {
			return rz_get_file_name(node);
		}
	}
	return NULL;
}

int bapi_pack_find_file(bapi_pack_t pack, const char *name)
{
	return pack_find_index(pack, name);
}

int64_t bapi_pack_file_size(bapi_pack_t pack, const char *name)
{
	rz_file_t *node = pack_find_node(pack, name);
	if (!node) return -1;
	/* Logical (decompressed) size; equals the stored size until compression
	 * is implemented. Sizes above INT64_MAX would wrap; unreachable in
	 * practice given the format's 4 GiB index_offset cap. */
	return (int64_t)rz_get_file_decomp_size(node);
}

size_t bapi_pack_read_file(bapi_pack_t pack, const char *name, void *buffer, size_t buffer_size)
{
	if (!pack || !name || !buffer || buffer_size == 0) return 0;
	rz_file_t *node = pack_find_node(pack, name);
	if (!node) return 0;
	return pack_copy_file(node, 0, (uint8_t *)buffer, buffer_size);
}

uint8_t *bapi_pack_read_file_alloc(bapi_pack_t pack, const char *name, size_t *out_size)
{
	rz_file_t *node = pack_find_node(pack, name);
	if (!node) return NULL;

	uint64_t size = rz_get_file_comp_size(node);
	if (size > (uint64_t)SIZE_MAX) return NULL;

	/* malloc(1) for empty entries: malloc(0) is not portable. */
	uint8_t *buffer = (uint8_t *)malloc(size > 0 ? (size_t)size : 1);
	if (!buffer) return NULL;

	size_t got = pack_copy_file(node, 0, buffer, (size_t)size);
	if (got != (size_t)size) {
		free(buffer);
		return NULL;
	}

	if (out_size) *out_size = (size_t)size;
	return buffer;
}

struct bapi_pack_stream_internal {
	bapi_pack_t pack;
	rz_file_t  *node;
	uint64_t	pos;
};

bapi_pack_stream_t bapi_pack_stream_open(bapi_pack_t pack, const char *name)
{
	rz_file_t *node = pack_find_node(pack, name);
	if (!node) return NULL;

	bapi_pack_stream_t stream =
		(bapi_pack_stream_t)malloc(sizeof(struct bapi_pack_stream_internal));
	if (!stream) return NULL;

	stream->pack = pack;
	stream->node = node;
	stream->pos	 = 0;
	return stream;
}

size_t bapi_pack_stream_read(bapi_pack_stream_t stream, void *buffer, size_t size)
{
	if (!stream || !buffer || size == 0) return 0;

	uint64_t total = rz_get_file_comp_size(stream->node);
	if (stream->pos >= total) return 0;

	uint64_t remaining = total - stream->pos;
	uint64_t want	   = remaining < (uint64_t)size ? remaining : (uint64_t)size;
	size_t	 got	   = pack_copy_file(stream->node, stream->pos, (uint8_t *)buffer, (size_t)want);
	stream->pos += got;
	return got;
}

int64_t bapi_pack_stream_seek(bapi_pack_stream_t stream, int64_t offset, int whence)
{
	if (!stream || (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)) return -1;

	uint64_t size = rz_get_file_comp_size(stream->node);
	uint64_t base;
	if (whence == SEEK_SET) {
		base = 0;
	} else if (whence == SEEK_CUR) {
		base = stream->pos;
	} else {
		base = size;
	}

	uint64_t target;
	if (offset < 0) {
		uint64_t magnitude = (uint64_t)(-(offset + 1)) + 1;
		target			   = magnitude <= base ? base - magnitude : 0;
	} else if (base > UINT64_MAX - (uint64_t)offset) {
		target = size; /* overflow: clamp to end */
	} else {
		target = base + (uint64_t)offset;
		if (target > size) target = size;
	}

	stream->pos = target;
	return (int64_t)target;
}

int64_t bapi_pack_stream_tell(bapi_pack_stream_t stream)
{
	if (!stream) return -1;
	return (int64_t)stream->pos;
}

int64_t bapi_pack_stream_size(bapi_pack_stream_t stream)
{
	if (!stream) return -1;
	return (int64_t)rz_get_file_comp_size(stream->node);
}

void bapi_pack_stream_close(bapi_pack_stream_t stream)
{
	free(stream);
}
