#include "BridgeEngine.h"

/* XJ380 has no writable filesystem, so gameplay serialization and manifest
 * verification report failure instead of pretending to work. */

int bapi_tilemap_save(bapi_tilemap_t tilemap, const char *filepath)
{
	(void)tilemap;
	(void)filepath;
	return -1;
}

bapi_tilemap_t bapi_tilemap_load(const char *filepath)
{
	(void)filepath;
	return NULL;
}

bapi_tilemap_t bapi_tilemap_load_from_memory(const char *text, size_t size)
{
	(void)text;
	(void)size;
	return NULL;
}

int bapi_world_save(bapi_world_t world, const char *filepath)
{
	(void)world;
	(void)filepath;
	return -1;
}

int bapi_world_load(bapi_world_t world, const char *filepath)
{
	(void)world;
	(void)filepath;
	return -1;
}

int bapi_world_load_from_memory(bapi_world_t world, const char *text, size_t size)
{
	(void)world;
	(void)text;
	(void)size;
	return -1;
}

bapi_asset_manifest_t bapi_asset_manifest_load(const char *filepath)
{
	(void)filepath;
	return NULL;
}

void bapi_asset_manifest_destroy(bapi_asset_manifest_t manifest)
{
	(void)manifest;
}

int bapi_asset_manifest_count(bapi_asset_manifest_t manifest)
{
	(void)manifest;
	return 0;
}

const char *bapi_asset_manifest_get(bapi_asset_manifest_t manifest, int index)
{
	(void)manifest;
	(void)index;
	return NULL;
}

int bapi_asset_manifest_verify(bapi_asset_manifest_t manifest, const char *root, int *checked)
{
	(void)manifest;
	(void)root;
	if (checked) *checked = 0;
	return 0;
}

const char *bapi_asset_manifest_first_missing(bapi_asset_manifest_t manifest)
{
	(void)manifest;
	return NULL;
}
