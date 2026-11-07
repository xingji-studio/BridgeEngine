#include "BridgeEngine.h"
#include "internal/platform/platform.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct plat_audio_device {
	int unused;
};

struct plat_audio_stream {
	int unused;
};

static struct plat_audio_device device;
static struct plat_audio_stream stream;
static int						queued_bytes;
static int						put_bytes;
static int						flush_count;
static int						clear_count;
static int						pause_count;
static int						resume_count;
static int						destroy_count;
static float					last_gain;
static int						gain_count;

static int expect(int condition, const char *message)
{
	if (!condition) fprintf(stderr, "%s\n", message);
	return condition;
}

static plat_audio_device_t open_audio_device(plat_audio_format_t format, int channels, int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return &device;
}

static void close_audio_device(plat_audio_device_t handle)
{
	(void)handle;
}

static plat_audio_stream_t create_audio_stream(const plat_audio_spec_t *spec)
{
	(void)spec;
	return &stream;
}

static plat_audio_stream_t open_audio_device_stream(plat_audio_format_t format, int channels,
													int freq)
{
	(void)format;
	(void)channels;
	(void)freq;
	return &stream;
}

static int bind_audio_stream(plat_audio_device_t handle, plat_audio_stream_t target)
{
	(void)handle;
	(void)target;
	return 0;
}

static void destroy_audio_stream(plat_audio_stream_t target)
{
	(void)target;
	destroy_count++;
}

static int put_audio_stream_data(plat_audio_stream_t target, const void *data, int len)
{
	(void)target;
	(void)data;
	put_bytes += len;
	queued_bytes += len;
	return 0;
}

static int flush_audio_stream(plat_audio_stream_t target)
{
	(void)target;
	flush_count++;
	return 0;
}

static int get_audio_stream_queued(plat_audio_stream_t target)
{
	(void)target;
	return queued_bytes;
}

static void clear_audio_stream(plat_audio_stream_t target)
{
	(void)target;
	queued_bytes = 0;
	clear_count++;
}

static int set_audio_stream_gain(plat_audio_stream_t target, float gain)
{
	(void)target;
	last_gain = gain;
	gain_count++;
	return 0;
}

static void resume_audio_stream_device(plat_audio_stream_t target)
{
	(void)target;
	resume_count++;
}

static void pause_audio_stream_device(plat_audio_stream_t target)
{
	(void)target;
	pause_count++;
}

static void mem_free(void *ptr)
{
	free(ptr);
}

static const plat_interface_t test_platform = {
	.capabilities = PLAT_CAPABILITY_AUDIO,
	.audio =
		{
			.open_audio_device			= open_audio_device,
			.close_audio_device			= close_audio_device,
			.create_audio_stream		= create_audio_stream,
			.open_audio_device_stream	= open_audio_device_stream,
			.bind_audio_stream			= bind_audio_stream,
			.destroy_audio_stream		= destroy_audio_stream,
			.put_audio_stream_data		= put_audio_stream_data,
			.flush_audio_stream			= flush_audio_stream,
			.get_audio_stream_queued	= get_audio_stream_queued,
			.clear_audio_stream			= clear_audio_stream,
			.set_audio_stream_gain		= set_audio_stream_gain,
			.resume_audio_stream_device = resume_audio_stream_device,
			.pause_audio_stream_device	= pause_audio_stream_device,
			.mem_free					= mem_free,
		},
};

const plat_interface_t *plat_get(void)
{
	return &test_platform;
}

int main(void)
{
	int result = 1;

	result &= expect(bapi_audio_init() == 0, "audio device opens through the mock backend");
	bapi_audio_set_master_volume(0.5f);
	result &= expect(fabsf(bapi_audio_get_master_volume() - 0.5f) < 0.0001f,
					 "master volume is stored");

	const char *wav = "examples/assets/audio/test.wav";
	bapi_music_t music = bapi_music_load(wav);
	if (music == NULL) {
		fprintf(stderr, "music load failed: %s\n", bapi_get_last_error());
		bapi_audio_cleanup();
		return 1;
	}
	result &= expect(bapi_music_get_duration(music) > 0.0, "music duration is reported");

	put_bytes = 0;
	result &= expect(bapi_music_play(music) == 0, "music play succeeds");
	result &= expect(put_bytes > 0, "play decodes and queues the first audio chunk");
	result &= expect(bapi_music_is_playing(music) == 1, "music reports playing");
	result &= expect(gain_count > 0 && fabsf(last_gain - 0.5f) < 0.0001f,
					 "music gain includes master volume");

	int before = put_bytes;
	bapi_music_update();
	result &= expect(put_bytes >= before, "update continues decoding");

	bapi_music_pause(music);
	result &= expect(pause_count > 0, "pause reaches the audio backend");
	result &= expect(bapi_music_is_playing(music) == 0, "paused music does not report playing");
	bapi_music_resume(music);
	result &= expect(resume_count > 0, "resume reaches the audio backend");
	result &= expect(bapi_music_is_playing(music) == 1, "resumed music reports playing");

	bapi_music_set_volume(music, 0.25f);
	result &= expect(fabsf(last_gain - 0.125f) < 0.0001f,
					 "music volume is multiplied by master volume");

	bapi_music_set_loop(music, 1);
	bapi_music_stop(music);
	result &= expect(clear_count > 0, "stop clears queued audio");
	result &= expect(bapi_music_is_playing(music) == 0, "stopped music reports stopped");

	bapi_music_free(music);
	result &= expect(destroy_count > 0, "music frees its backend stream");

	FILE *file = fopen(wav, "rb");
	if (file == NULL) {
		fprintf(stderr, "cannot reopen %s\n", wav);
		bapi_audio_cleanup();
		return 1;
	}
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t *bytes = malloc((size_t)length);
	if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
		fprintf(stderr, "cannot read %s\n", wav);
		free(bytes);
		fclose(file);
		bapi_audio_cleanup();
		return 1;
	}
	fclose(file);

	bapi_music_t memory_music = bapi_music_load_from_memory(bytes, (size_t)length);
	free(bytes);
	result &= expect(memory_music != NULL, "music loads from memory");
	result &= expect(bapi_music_play(memory_music) == 0, "memory music plays");
	result &= expect(bapi_music_get_duration(memory_music) > 0.0, "memory music duration");
	bapi_music_free(memory_music);

	bapi_audio_cleanup();
	return result ? 0 : 1;
}
