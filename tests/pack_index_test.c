#include "BridgeEngine.h"
#include "rz_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENTRY_COUNT 4096
#define NAME_LENGTH 14

static int	g_fails;
static void expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		g_fails++;
	}
}
static void put_le(unsigned char *out, uint64_t value, int bytes)
{
	for (int i = 0; i < bytes; i++) {
		out[i] = (unsigned char)value;
		value >>= 8;
	}
}
static uint32_t byte_crc(unsigned char byte)
{
	uint32_t crc = 0xffffffffu ^ byte;
	for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
	return ~crc;
}
static void entry_name(int index, char name[NAME_LENGTH + 1])
{
	/* Reverse lexical order; the final entry duplicates the first name but has
	 * different bytes, so sorted lookups must preserve original ordinals. */
	unsigned int number = ENTRY_COUNT - 1u - (unsigned int)index % ENTRY_COUNT;
	snprintf(name, NAME_LENGTH + 1, "entry-%08u", number);
}
static int write_fixture(const char *path)
{
	FILE *file = fopen(path, "wb");
	if (!file) return 0;
	unsigned char	   header[48] = {'R', 'Z'};
	const unsigned int count	  = ENTRY_COUNT + 1;
	put_le(header + 2, RZ_VERSION_MAJOR, 2);
	put_le(header + 4, RZ_VERSION_MINOR, 2);
	put_le(header + 6, count, 8);
	put_le(header + 14, 48u + count * (1u + 38u + NAME_LENGTH), 8);
	put_le(header + 22, count, 4);
	put_le(header + 30, 48u + count, 4);
	fwrite(header, 1, sizeof(header), file);
	for (int i = 0; i <= ENTRY_COUNT; i++) fputc(i == ENTRY_COUNT ? 255 : i % 251, file);
	for (int i = 0; i <= ENTRY_COUNT; i++) {
		unsigned char record[38 + NAME_LENGTH] = {0};
		char		  name[NAME_LENGTH + 1];
		entry_name(i, name);
		put_le(record, NAME_LENGTH, 2);
		memcpy(record + 2, name, NAME_LENGTH);
		put_le(record + 2 + NAME_LENGTH, 1, 8);
		put_le(record + 10 + NAME_LENGTH, 1, 8);
		put_le(record + 18 + NAME_LENGTH, 48u + (unsigned int)i, 8);
		put_le(record + 26 + NAME_LENGTH,
			   byte_crc((unsigned char)(i == ENTRY_COUNT ? 255 : i % 251)), 4);
		fwrite(record, 1, sizeof(record), file);
	}
	int ok = !ferror(file);
	if (fclose(file) != 0) ok = 0;
	return ok;
}
int main(int argc, char **argv)
{
	if (argc < 2) return 1;
	const char *path = argv[1];
	if (!write_fixture(path)) return 1;
	expect(rz_test(path) == 0, "fixture passes RZip structural and CRC validation");
	bapi_pack_t pack = bapi_pack_open(path);
	if (!pack) {
		remove(path);
		return 1;
	}
	expect(bapi_pack_file_count(pack) == ENTRY_COUNT + 1, "indexed pack count");
	for (int i = 0; i < ENTRY_COUNT; i++) {
		char name[NAME_LENGTH + 1];
		entry_name(i, name);
		const char *actual = bapi_pack_file_name(pack, i);
		expect(actual && strcmp(actual, name) == 0, "enumeration preserves archive order");
		expect(bapi_pack_find_file(pack, name) == i, "lookup returns original index");
		expect(bapi_pack_file_size(pack, name) == 1, "size lookup uses correct entry");
		unsigned char byte = 255;
		expect(bapi_pack_read_file(pack, name, &byte, 1) == 1 && byte == i % 251,
			   "read lookup selects original data, including first duplicate");
	}
	char first[NAME_LENGTH + 1];
	entry_name(0, first);
	expect(strcmp(bapi_pack_file_name(pack, ENTRY_COUNT), first) == 0, "duplicate is enumerated");
	expect(bapi_pack_find_file(pack, "") == -1 && bapi_pack_find_file(pack, "aaa") == -1 &&
			   bapi_pack_find_file(pack, "entry-00000000x") == -1 &&
			   bapi_pack_find_file(pack, "zzz") == -1,
		   "missing names at all binary search boundaries");
	expect(!bapi_pack_file_name(pack, ENTRY_COUNT + 1) && !bapi_pack_file_name(pack, -1),
		   "enumeration bounds");
	bapi_pack_stream_t stream = bapi_pack_stream_open(pack, first);
	unsigned char	   byte	  = 255;
	expect(stream && bapi_pack_stream_read(stream, &byte, 1) == 1 && byte == 0,
		   "stream lookup selects first duplicate");
	bapi_pack_stream_close(stream);
	if (argc == 3 && strcmp(argv[2], "--benchmark") == 0) {
		volatile int checksum = 0;
		clock_t		 start	  = clock();
		for (int i = 0; i < 50000; i++) {
			char name[NAME_LENGTH + 1];
			entry_name((i * 37) % ENTRY_COUNT, name);
			checksum += bapi_pack_find_file(pack, name);
		}
		double lookup = (double)(clock() - start) / CLOCKS_PER_SEC;
		start		  = clock();
		for (int repeat = 0; repeat < 20; repeat++)
			for (int i = 0; i < ENTRY_COUNT; i++) checksum += bapi_pack_file_name(pack, i)[0];
		printf("pack: entries=%d lookups=50000 lookup_seconds=%.6f enumerate_seconds=%.6f "
			   "checksum=%d\n",
			   ENTRY_COUNT + 1, lookup, (double)(clock() - start) / CLOCKS_PER_SEC, checksum);
	}
	bapi_pack_close(pack);
	remove(path);
	return g_fails ? 1 : 0;
}
