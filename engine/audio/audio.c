#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio/audio.h"
//The file audio.c is very hard to write,so f*** libsdl(
//Don't tinker with it unless you know what you are doing!!!
struct bapi_sound_internal {
    SDL_AudioSpec spec;
    Uint8* buffer;
    Uint32 length;
    float volume;
    int loop;
    int playing;
};

static SDL_AudioStream* audio_stream = NULL;
static bapi_sound_t current_playing = NULL;
//bapi audio init
int bapi_audio_init(void) {
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 44100;
    
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (audio_stream == NULL) {
        printf("[AUDIO] Failed to open audio device: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_ResumeAudioStreamDevice(audio_stream);
    printf("[AUDIO] Audio device opened successfully\n");
    return 0;
}

void bapi_audio_cleanup(void) {
    if (audio_stream != NULL) {
        SDL_DestroyAudioStream(audio_stream);
        audio_stream = NULL;
    }
    current_playing = NULL;
}

bapi_sound_t bapi_sound_load(const char* filepath) {
    bapi_sound_t sound = malloc(sizeof(struct bapi_sound_internal));
    if (sound == NULL) {
        return NULL;
    }
    
    SDL_zero(sound->spec);
    if (!SDL_LoadWAV(filepath, &sound->spec, &sound->buffer, &sound->length)) {
        printf("[AUDIO] Failed to load WAV %s: %s\n", filepath, SDL_GetError());
        free(sound);
        return NULL;
    }
    
    printf("[AUDIO] Loaded WAV: format=%d, channels=%d, freq=%d, length=%u\n",
           sound->spec.format, sound->spec.channels, sound->spec.freq, sound->length);
    
    sound->volume = 1.0f;
    sound->loop = 0;
    sound->playing = 0;
    return sound;
}

static int put_audio_data(bapi_sound_t sound) {
    if (!SDL_SetAudioStreamFormat(audio_stream, &sound->spec, NULL)) {
        printf("[AUDIO] Failed to set audio stream format: %s\n", SDL_GetError());
        return 1;
    }
    
    if (sound->volume < 0.99f) {
        Uint32 bytes_per_sample = SDL_AUDIO_BYTESIZE(sound->spec.format) * sound->spec.channels;
        Uint32 sample_count = sound->length / bytes_per_sample;
        Uint8* temp_buffer = malloc(sound->length);
        if (temp_buffer == NULL) {
            return 1;
        }
        
        memcpy(temp_buffer, sound->buffer, sound->length);
        
        if (sound->spec.format == SDL_AUDIO_F32) {
            float* samples = (float*)temp_buffer;
            for (Uint32 i = 0; i < sample_count; i++) {
                samples[i] *= sound->volume;
            }
        } else if (sound->spec.format == SDL_AUDIO_S16) {
            Sint16* samples = (Sint16*)temp_buffer;
            for (Uint32 i = 0; i < sample_count; i++) {
                samples[i] = (Sint16)(samples[i] * sound->volume);
            }
        } else if (sound->spec.format == SDL_AUDIO_S32) {
            Sint32* samples = (Sint32*)temp_buffer;
            for (Uint32 i = 0; i < sample_count; i++) {
                samples[i] = (Sint32)(samples[i] * sound->volume);
            }
        }
        
        int result = SDL_PutAudioStreamData(audio_stream, temp_buffer, sound->length);
        free(temp_buffer);
        if (!result) {
            printf("[AUDIO] Failed to put audio data: %s\n", SDL_GetError());
            return 1;
        }
        return 0;
    }
    
    if (!SDL_PutAudioStreamData(audio_stream, sound->buffer, sound->length)) {
        printf("[AUDIO] Failed to put audio data: %s\n", SDL_GetError());
        return 1;
    }
    
    return 0;
}

int bapi_sound_play(bapi_sound_t sound) {
    if (sound == NULL || audio_stream == NULL) {
        printf("[AUDIO] Play failed: sound=%p, stream=%p\n", (void*)sound, (void*)audio_stream);
        return 1;
    }
    
    sound->playing = 1;
    current_playing = sound;
    
    printf("[AUDIO] Playing sound, length=%u, loop=%d\n", sound->length, sound->loop);
    return put_audio_data(sound);
}

void bapi_sound_set_volume(bapi_sound_t sound, float volume) {
    if (sound != NULL) {
        if (volume < 0.0f) {
            sound->volume = 0.0f;
        } else if (volume > 1.0f) {
            sound->volume = 1.0f;
        } else {
            sound->volume = volume;
        }
    }
}
//for
void bapi_sound_set_loop(bapi_sound_t sound, int loop) {
    if (sound != NULL) {
        sound->loop = loop;
    }
}

void bapi_sound_stop(bapi_sound_t sound) {
    if (sound != NULL) {
        sound->playing = 0;
        if (current_playing == sound) {
            SDL_ClearAudioStream(audio_stream);
            current_playing = NULL;
        }
    }
}

void bapi_sound_update(void) {
    if (current_playing != NULL && current_playing->playing && current_playing->loop) {
        int available = SDL_GetAudioStreamAvailable(audio_stream);
        if (available < (int)(current_playing->length / 4)) {
            put_audio_data(current_playing);
        }
    }
}

void bapi_sound_free(bapi_sound_t sound) {
    if (sound != NULL) {
        if (sound->buffer != NULL) {
            SDL_free(sound->buffer);
        }
        if (current_playing == sound) {
            current_playing = NULL;
        }
        free(sound);
    }
}
