#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/platform/platform.h"
#include <stdint.h>
#include <stdlib.h>

struct bapi_file_internal {
	plat_io_t *io;
};

bapi_file_t bapi_file_open(const char *path)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !path) return NULL;
	plat_io_t *io = plat->io.open_read(path);
	if (!io) {
		bapi_set_last_error("file could not be opened: %s", path);
		return NULL;
	}
	bapi_file_t file = (bapi_file_t)malloc(sizeof(struct bapi_file_internal));
	if (!file) {
		plat->io.close(io);
		return NULL;
	}
	file->io = io;
	return file;
}

size_t bapi_file_read(bapi_file_t file, void *buffer, size_t size)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !file || !buffer) return 0;
	return plat->io.read(file->io, buffer, size);
}

int64_t bapi_file_seek(bapi_file_t file, int64_t offset, int origin)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !file) return -1;
	return plat->io.seek(file->io, offset, origin);
}

int64_t bapi_file_tell(bapi_file_t file)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !file) return -1;
	return plat->io.tell(file->io);
}

int64_t bapi_file_size(bapi_file_t file)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !file) return -1;
	return plat->io.size(file->io);
}

void bapi_file_close(bapi_file_t file)
{
	const plat_interface_t *plat = plat_get();
	if (plat && file) {
		plat->io.close(file->io);
		free(file);
	}
}

uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size)
{
	const plat_interface_t *plat = plat_get();
	if (!plat || !path || !plat->io.open_read || !plat->io.read) return NULL;

	plat_io_t *io = plat->io.open_read(path);
	if (!io) {
		bapi_set_last_error("file could not be opened: %s", path);
		return NULL;
	}

	size_t capacity = 4096;
	size_t length	= 0;
	uint8_t *buffer = (uint8_t *)malloc(capacity);
	if (!buffer) {
		plat->io.close(io);
		return NULL;
	}

	for (;;) {
		size_t got = plat->io.read(io, buffer + length, capacity - length);
		if (got == 0) break; /* EOF */
		length += got;
		if (length < capacity) continue;

		if (capacity > (size_t)-1 / 2) { /* would overflow the grow */
			plat->io.close(io);
			free(buffer);
			return NULL;
		}
		capacity *= 2;
		uint8_t *grown = (uint8_t *)realloc(buffer, capacity);
		if (!grown) {
			plat->io.close(io);
			free(buffer);
			return NULL;
		}
		buffer = grown;
	}

	plat->io.close(io);

	/* malloc(1) for empty files: malloc(0) is not portable. A failed shrink is
	 * non-fatal: keep the larger buffer. */
	uint8_t *shrunk = (uint8_t *)realloc(buffer, length > 0 ? length : 1);
	if (shrunk) buffer = shrunk;
	if (out_size) *out_size = length;
	return buffer;
}