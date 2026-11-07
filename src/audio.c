#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bapi_sound_internal {
	plat_audio_spec_t			spec;
	uint8_t					   *buffer;
	uint32_t					length;
	float						volume;
	int							loop;
	int							playing;
	plat_audio_stream_t			stream;
	int							queued_once;
	struct bapi_sound_internal *next_active;
	struct bapi_sound_internal *next_allocated;
};

static plat_audio_device_t audio_device	 = NULL;
static bapi_sound_t		   active_sounds = NULL;
static bapi_sound_t		   allocated_sounds = NULL;
static float			   g_master_volume = 1.0f;
static int				   audio_unsupported_warning_logged;

plat_audio_device_t bapi_audio_device_handle(void)
{
	return audio_device;
}

float bapi_audio_master_volume(void)
{
	return g_master_volume;
}

static int audio_is_supported(void)
{
	return plat_supports(PLAT_CAPABILITY_AUDIO);
}

static void warn_audio_unsupported_once(const plat_interface_t *plat)
{
	if (!audio_unsupported_warning_logged && plat != NULL && plat->core.log_warn != NULL) {
		plat->core.log_warn("Audio is not supported by this platform");
		audio_unsupported_warning_logged = 1;
	}
}

static void remove_active_sound(bapi_sound_t sound)
{
	bapi_sound_t *current = &active_sounds;
	while (*current != NULL) {
		if (*current == sound) {
			*current		   = sound->next_active;
			sound->next_active = NULL;
			return;
		}
		current = &(*current)->next_active;
	}
}

static void add_active_sound(bapi_sound_t sound)
{
	if (sound == NULL || sound->next_active != NULL || active_sounds == sound) {
		return;
	}

	sound->next_active = active_sounds;
	active_sounds	   = sound;
}

static void remove_allocated_sound(bapi_sound_t sound)
{
	bapi_sound_t *current = &allocated_sounds;
	while (*current != NULL) {
		if (*current == sound) {
			*current = sound->next_allocated;
			return;
		}
		current = &(*current)->next_allocated;
	}
}

static int ensure_sound_stream(bapi_sound_t sound)
{
	if (!audio_is_supported()) return 1;

	if (sound->stream != NULL) {
		return 0;
	}

	const plat_interface_t *plat = plat_get();

	sound->stream = plat->audio.create_audio_stream(&sound->spec);
	if (sound->stream == NULL) {
		printf("[AUDIO] Failed to create sound stream\n");
		return 1;
	}

	if (plat->audio.bind_audio_stream(audio_device, sound->stream) != 0) {
		printf("[AUDIO] Failed to bind sound stream\n");
		plat->audio.destroy_audio_stream(sound->stream);
		sound->stream = NULL;
		return 1;
	}

	if (plat->audio.set_audio_stream_gain(sound->stream, sound->volume * g_master_volume) != 0) {
		printf("[AUDIO] Failed to set sound gain\n");
		plat->audio.destroy_audio_stream(sound->stream);
		sound->stream = NULL;
		return 1;
	}

	return 0;
}

static int queue_sound_buffer(bapi_sound_t sound)
{
	if (!audio_is_supported()) return 1;

	const plat_interface_t *plat = plat_get();

	if (plat->audio.put_audio_stream_data(sound->stream, sound->buffer, (int)sound->length) != 0) {
		printf("[AUDIO] Failed to queue audio data\n");
		return 1;
	}

	if (plat->audio.flush_audio_stream(sound->stream) != 0) {
		printf("[AUDIO] Failed to flush audio stream\n");
		return 1;
	}

	return 0;
}

int bapi_audio_init(void)
{
	const plat_interface_t *plat = plat_get();
	if (!audio_is_supported() || plat->audio.open_audio_device == NULL) {
		warn_audio_unsupported_once(plat);
		return 1;
	}

	audio_device = plat->audio.open_audio_device(PLAT_AUDIO_F32, 2, 44100);
	if (audio_device == NULL) {
		printf("[AUDIO] Failed to open audio device\n");
		return 1;
	}

	printf("[AUDIO] Audio device opened successfully\n");
	return 0;
}

void bapi_audio_cleanup(void)
{
	if (!audio_is_supported()) return;

	const plat_interface_t *plat = plat_get();

	bapi_music_cleanup();
	while (allocated_sounds != NULL) {
		bapi_sound_free(allocated_sounds);
	}
	active_sounds = NULL;

	if (audio_device != NULL) {
		plat->audio.close_audio_device(audio_device);
		audio_device = NULL;
	}
	g_master_volume = 1.0f;
}

void bapi_audio_set_master_volume(float volume)
{
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	g_master_volume = volume;

	if (!audio_is_supported()) return;
	const plat_interface_t *plat = plat_get();
	for (bapi_sound_t sound = allocated_sounds; sound != NULL; sound = sound->next_allocated) {
		if (sound->stream != NULL) {
			plat->audio.set_audio_stream_gain(sound->stream, sound->volume * g_master_volume);
		}
	}
	bapi_music_apply_master_volume(g_master_volume);
}

float bapi_audio_get_master_volume(void)
{
	return g_master_volume;
}

void bapi_audio_stop_all(void)
{
	if (!audio_is_supported()) return;
	bapi_sound_t sound = active_sounds;
	while (sound != NULL) {
		bapi_sound_t next = sound->next_active;
		bapi_sound_stop(sound);
		sound = next;
	}
}

int bapi_audio_get_active_sound_count(void)
{
	int count = 0;
	for (bapi_sound_t sound = active_sounds; sound != NULL; sound = sound->next_active) count++;
	return count;
}

static bapi_sound_t sound_from_pcm(plat_audio_spec_t *spec, uint8_t *buffer, uint32_t length)
{
	bapi_sound_t sound = malloc(sizeof(struct bapi_sound_internal));
	if (sound == NULL) {
		if (buffer != NULL) {
			const plat_interface_t *plat = plat_get();
			if (plat != NULL && plat->audio.mem_free != NULL) {
				plat->audio.mem_free(buffer);
			} else {
				free(buffer);
			}
		}
		return NULL;
	}

	memcpy(&sound->spec, spec, sizeof(plat_audio_spec_t));
	sound->buffer = buffer;
	sound->length = length;

	printf("[AUDIO] Loaded WAV: channels=%d, freq=%d, length=%u\n", sound->spec.channels,
		   sound->spec.freq, sound->length);

	sound->volume	   = 1.0f;
	sound->loop		   = 0;
	sound->playing	   = 0;
	sound->stream	   = NULL;
	sound->queued_once = 0;
	sound->next_active = NULL;
	sound->next_allocated = allocated_sounds;
	allocated_sounds = sound;
	return sound;
}

bapi_sound_t bapi_sound_load(const char *filepath)
{
	const plat_interface_t *plat = plat_get();
	if (!audio_is_supported()) {
		warn_audio_unsupported_once(plat);
		return NULL;
	}

	if (!filepath) return NULL;

	plat_audio_spec_t spec;
	uint8_t			 *buffer = NULL;
	uint32_t		  length = 0;

	memset(&spec, 0, sizeof(spec));
	if (plat->io.open_read && plat->io.read && plat->audio.load_wav_mem) {
		/* Two-stage load: read the file into memory, decode from memory. */
		size_t	 size = 0;
		uint8_t *data = bapi_file_read_alloc(filepath, &size);
		if (!data) {
			printf("[AUDIO] Failed to read WAV file %s\n", filepath);
			bapi_set_last_error("audio file could not be read: %s", filepath);
			return NULL;
		}
		int result = plat->audio.load_wav_mem(data, size, &spec, &buffer, &length);
		free(data);
		if (result != 0) {
			printf("[AUDIO] Failed to load WAV %s\n", filepath);
			bapi_set_last_error("WAV file could not be decoded: %s", filepath);
			return NULL;
		}
	} else if (plat->audio.load_wav) {
		if (plat->audio.load_wav(filepath, &spec, &buffer, &length) != 0) {
			printf("[AUDIO] Failed to load WAV %s\n", filepath);
			bapi_set_last_error("WAV file could not be decoded: %s", filepath);
			return NULL;
		}
	} else {
		warn_audio_unsupported_once(plat);
		return NULL;
	}

	return sound_from_pcm(&spec, buffer, length);
}

bapi_sound_t bapi_sound_load_from_memory(const void *data, size_t size)
{
	const plat_interface_t *plat = plat_get();
	if (!audio_is_supported() || plat->audio.load_wav_mem == NULL) {
		warn_audio_unsupported_once(plat);
		return NULL;
	}

	if (!data || size == 0) return NULL;

	plat_audio_spec_t spec;
	uint8_t			 *buffer = NULL;
	uint32_t		  length = 0;

	memset(&spec, 0, sizeof(spec));
	if (plat->audio.load_wav_mem(data, size, &spec, &buffer, &length) != 0) {
		printf("[AUDIO] Failed to decode WAV from memory\n");
		bapi_set_last_error("WAV data could not be decoded from memory");
		return NULL;
	}

	return sound_from_pcm(&spec, buffer, length);
}

int bapi_sound_play(bapi_sound_t sound)
{
	if (!audio_is_supported()) return 1;

	if (sound == NULL || audio_device == NULL) {
		printf("[AUDIO] Play failed: sound=%p, device=%p\n", (void *)sound, (void *)audio_device);
		return 1;
	}

	if (ensure_sound_stream(sound) != 0) {
		return 1;
	}

	const plat_interface_t *plat = plat_get();
	plat->audio.clear_audio_stream(sound->stream);

	sound->playing	   = 1;
	sound->queued_once = 0;
	add_active_sound(sound);

	printf("[AUDIO] Playing sound, length=%u, loop=%d\n", sound->length, sound->loop);
	if (queue_sound_buffer(sound) != 0) {
		sound->playing = 0;
		remove_active_sound(sound);
		return 1;
	}

	sound->queued_once = 1;
	return 0;
}

void bapi_sound_set_volume(bapi_sound_t sound, float volume)
{
	if (!audio_is_supported()) return;

	if (sound != NULL) {
		const plat_interface_t *plat = plat_get();

		if (volume < 0.0f) {
			sound->volume = 0.0f;
		} else if (volume > 1.0f) {
			sound->volume = 1.0f;
		} else {
			sound->volume = volume;
		}

		if (sound->stream != NULL) {
			plat->audio.set_audio_stream_gain(sound->stream, sound->volume * g_master_volume);
		}
	}
}

void bapi_sound_set_loop(bapi_sound_t sound, int loop)
{
	if (!audio_is_supported()) return;

	if (sound != NULL) {
		sound->loop = loop;
	}
}

void bapi_sound_stop(bapi_sound_t sound)
{
	if (!audio_is_supported()) return;

	if (sound != NULL) {
		const plat_interface_t *plat = plat_get();

		sound->playing	   = 0;
		sound->queued_once = 0;
		if (sound->stream != NULL) {
			plat->audio.clear_audio_stream(sound->stream);
		}
		remove_active_sound(sound);
	}
}

void bapi_sound_update(void)
{
	if (!audio_is_supported()) return;

	const plat_interface_t *plat  = plat_get();
	bapi_sound_t			sound = active_sounds;

	while (sound != NULL) {
		bapi_sound_t next = sound->next_active;

		if (!sound->playing || sound->stream == NULL) {
			remove_active_sound(sound);
			sound = next;
			continue;
		}

		int queued = plat->audio.get_audio_stream_queued(sound->stream);
		if (queued < 0) {
			printf("[AUDIO] Failed to query queued audio data\n");
			sound->playing = 0;
			remove_active_sound(sound);
			sound = next;
			continue;
		}

		if (sound->loop) {
			if (queued <= (int)(sound->length / 2) && queue_sound_buffer(sound) != 0) {
				sound->playing = 0;
				remove_active_sound(sound);
			}
		} else if (sound->queued_once && queued == 0) {
			sound->playing	   = 0;
			sound->queued_once = 0;
			remove_active_sound(sound);
		}

		sound = next;
	}
}

void bapi_sound_free(bapi_sound_t sound)
{
	if (!audio_is_supported()) return;

	if (sound != NULL) {
		const plat_interface_t *plat = plat_get();

		bapi_sound_stop(sound);
		if (sound->buffer != NULL) {
			plat->audio.mem_free(sound->buffer);
		}
		if (sound->stream != NULL) {
			plat->audio.destroy_audio_stream(sound->stream);
		}
		remove_allocated_sound(sound);
		free(sound);
	}
}
