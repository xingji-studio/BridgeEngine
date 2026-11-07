#include "BridgeEngine.h"
#include "internal/bapi_internal.h"

/* Audio (and therefore streaming music) is unavailable on this backend. */

bapi_music_t bapi_music_load(const char *filepath)
{
	(void)filepath;
	bapi_set_last_error("streaming music is not supported by this platform");
	return NULL;
}

bapi_music_t bapi_music_load_from_memory(const void *data, size_t size)
{
	(void)data;
	(void)size;
	bapi_set_last_error("streaming music is not supported by this platform");
	return NULL;
}

void bapi_music_free(bapi_music_t music)
{
	(void)music;
}

int bapi_music_play(bapi_music_t music)
{
	(void)music;
	return 1;
}

void bapi_music_pause(bapi_music_t music)
{
	(void)music;
}

void bapi_music_resume(bapi_music_t music)
{
	(void)music;
}

void bapi_music_stop(bapi_music_t music)
{
	(void)music;
}

void bapi_music_set_volume(bapi_music_t music, float volume)
{
	(void)music;
	(void)volume;
}

void bapi_music_set_loop(bapi_music_t music, int loop)
{
	(void)music;
	(void)loop;
}

int bapi_music_is_playing(bapi_music_t music)
{
	(void)music;
	return 0;
}

double bapi_music_get_duration(bapi_music_t music)
{
	(void)music;
	return 0.0;
}

void bapi_music_update(void) {}

void bapi_music_apply_master_volume(float master)
{
	(void)master;
}

void bapi_music_cleanup(void) {}
