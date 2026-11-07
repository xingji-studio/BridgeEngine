#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/platform/platform.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MUSIC_SAMPLE_RATE 44100
#define MUSIC_CHANNELS	  2
#define MUSIC_QUEUE_BYTES (MUSIC_SAMPLE_RATE * MUSIC_CHANNELS * (int)sizeof(float) * 3 / 4)
#define MUSIC_MAX_PACKETS 8

typedef struct {
	const uint8_t *data;
	size_t			size;
	int64_t			position;
} music_io_t;

struct bapi_music_internal {
	AVFormatContext *format;
	AVCodecContext	*codec;
	SwrContext		*swr;
	AVPacket		*packet;
	AVFrame			*frame;
	AVIOContext		*avio;
	uint8_t			*avio_buffer;
	uint8_t			*memory;
	size_t			 memory_size;
	music_io_t		 io;
	int				 custom_io;
	int				 audio_stream_index;
	plat_audio_stream_t stream;
	float			 volume;
	int				 loop;
	int				 playing;
	int				 paused;
	int				 eof;
	double			 duration;
	struct bapi_music_internal *next_allocated;
};

static bapi_music_t g_allocated_music;

static int music_io_read(void *opaque, uint8_t *buffer, int size)
{
	music_io_t *io = opaque;
	if (io->position >= (int64_t)io->size) return AVERROR_EOF;
	size_t remaining = io->size - (size_t)io->position;
	size_t amount	 = (size_t)size < remaining ? (size_t)size : remaining;
	memcpy(buffer, io->data + io->position, amount);
	io->position += (int64_t)amount;
	return (int)amount;
}

static int64_t music_io_seek(void *opaque, int64_t offset, int whence)
{
	music_io_t *io = opaque;
	if (whence == AVSEEK_SIZE) return (int64_t)io->size;

	int64_t target = offset;
	if (whence == SEEK_CUR) target = io->position + offset;
	else if (whence == SEEK_END) target = (int64_t)io->size + offset;
	else if (whence != SEEK_SET) return -1;

	if (target < 0 || target > (int64_t)io->size) return -1;
	io->position = target;
	return target;
}

static void music_remove_allocated(bapi_music_t music)
{
	bapi_music_t *current = &g_allocated_music;
	while (*current != NULL) {
		if (*current == music) {
			*current			  = music->next_allocated;
			music->next_allocated = NULL;
			return;
		}
		current = &(*current)->next_allocated;
	}
}

static int music_seek_start(bapi_music_t music)
{
	if (music->format == NULL || music->codec == NULL) return -1;
	if (av_seek_frame(music->format, music->audio_stream_index, 0, AVSEEK_FLAG_BACKWARD) < 0) {
		return -1;
	}
	avcodec_flush_buffers(music->codec);
	if (music->swr != NULL) {
		swr_close(music->swr);
		if (swr_init(music->swr) < 0) return -1;
	}
	music->eof = 0;
	return 0;
}

static void music_apply_gain(bapi_music_t music)
{
	if (music->stream == NULL) return;
	const plat_interface_t *plat = plat_get();
	if (plat != NULL && plat->audio.set_audio_stream_gain != NULL) {
		plat->audio.set_audio_stream_gain(music->stream,
										  music->volume * bapi_audio_master_volume());
	}
}

static void music_decode_some(bapi_music_t music)
{
	const plat_interface_t *plat = plat_get();
	if (plat == NULL || music->stream == NULL) return;

	int queued = plat->audio.get_audio_stream_queued(music->stream);
	if (queued < 0) queued = 0;

	int packets = 0;
	while (queued < MUSIC_QUEUE_BYTES && packets < MUSIC_MAX_PACKETS) {
		int ret = av_read_frame(music->format, music->packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF && music->loop) {
				if (music_seek_start(music) != 0) {
					music->eof = 1;
					break;
				}
				continue;
			}
			music->eof = 1;
			break;
		}
		packets++;

		if (music->packet->stream_index != music->audio_stream_index) {
			av_packet_unref(music->packet);
			continue;
		}

		ret = avcodec_send_packet(music->codec, music->packet);
		av_packet_unref(music->packet);
		if (ret < 0 && ret != AVERROR(EAGAIN)) {
			music->eof = 1;
			break;
		}

		for (;;) {
			ret = avcodec_receive_frame(music->codec, music->frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			if (ret < 0) {
				music->eof = 1;
				break;
			}

			int out_samples = (int)av_rescale_rnd(
				swr_get_delay(music->swr, music->codec->sample_rate) + music->frame->nb_samples,
				MUSIC_SAMPLE_RATE, music->codec->sample_rate, AV_ROUND_UP);
			if (out_samples > 0) {
				int			out_bytes = out_samples * MUSIC_CHANNELS * (int)sizeof(float);
				uint8_t	   *out		  = malloc((size_t)out_bytes);
				if (out != NULL) {
					uint8_t *out_data[1] = {out};
					int		 converted	 = swr_convert(music->swr, out_data, out_samples,
														(const uint8_t **)music->frame->extended_data,
														music->frame->nb_samples);
					if (converted > 0) {
						int bytes = converted * MUSIC_CHANNELS * (int)sizeof(float);
						plat->audio.put_audio_stream_data(music->stream, out, bytes);
						plat->audio.flush_audio_stream(music->stream);
						queued += bytes;
					}
					free(out);
				}
			}
			av_frame_unref(music->frame);
			if (queued >= MUSIC_QUEUE_BYTES) break;
		}
	}
}

static void music_close(bapi_music_t music)
{
	if (music == NULL) return;
	if (music->stream != NULL) {
		const plat_interface_t *plat = plat_get();
		if (plat != NULL && plat->audio.destroy_audio_stream != NULL) {
			plat->audio.destroy_audio_stream(music->stream);
		}
		music->stream = NULL;
	}
	if (music->swr != NULL) swr_free(&music->swr);
	if (music->frame != NULL) av_frame_free(&music->frame);
	if (music->packet != NULL) av_packet_free(&music->packet);
	if (music->codec != NULL) avcodec_free_context(&music->codec);
	if (music->format != NULL) {
		if (music->custom_io) {
			music->format->pb = NULL;
		}
		avformat_close_input(&music->format);
	}
	if (music->avio != NULL) {
		av_freep(&music->avio->buffer);
		avio_context_free(&music->avio);
		music->avio = NULL;
	} else if (music->avio_buffer != NULL) {
		av_freep(&music->avio_buffer);
	}
	music->avio_buffer = NULL;
	free(music->memory);
	music->memory = NULL;
}

static bapi_music_t music_create(void)
{
	bapi_music_t music = calloc(1, sizeof(*music));
	if (music == NULL) {
		bapi_set_last_error("out of memory while creating music");
		return NULL;
	}
	music->volume = 1.0f;
	music->audio_stream_index = -1;
	music->next_allocated	  = g_allocated_music;
	g_allocated_music		  = music;
	return music;
}

static int music_open(bapi_music_t music, const char *filepath, const void *data, size_t size)
{
	if (data != NULL && size > 0) {
		music->memory = malloc(size);
		if (music->memory == NULL) {
			bapi_set_last_error("out of memory while copying music bytes");
			return -1;
		}
		memcpy(music->memory, data, size);
		music->memory_size = size;
		music->io.data	   = music->memory;
		music->io.size	   = size;
		music->io.position = 0;

		music->avio_buffer = av_malloc(4096);
		if (music->avio_buffer == NULL) {
			bapi_set_last_error("out of memory while creating music IO");
			return -1;
		}
		music->avio = avio_alloc_context(music->avio_buffer, 4096, 0, &music->io, music_io_read,
										 NULL, music_io_seek);
		if (music->avio == NULL) {
			bapi_set_last_error("cannot create music IO");
			return -1;
		}
		music->format = avformat_alloc_context();
		if (music->format == NULL) {
			bapi_set_last_error("cannot allocate music format context");
			return -1;
		}
		music->format->pb	 = music->avio;
		music->format->flags |= AVFMT_FLAG_CUSTOM_IO;
		music->custom_io	 = 1;
		if (avformat_open_input(&music->format, NULL, NULL, NULL) < 0) {
			bapi_set_last_error("music data could not be opened");
			return -1;
		}
	} else {
		if (filepath == NULL || filepath[0] == '\0') {
			bapi_set_last_error("music filepath is empty");
			return -1;
		}
		if (avformat_open_input(&music->format, filepath, NULL, NULL) < 0) {
			bapi_set_last_error("music file could not be opened: %s", filepath);
			return -1;
		}
	}

	if (avformat_find_stream_info(music->format, NULL) < 0) {
		bapi_set_last_error("music stream info could not be read");
		return -1;
	}

	const AVCodec *decoder = NULL;
	music->audio_stream_index =
		av_find_best_stream(music->format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
	if (music->audio_stream_index < 0 || decoder == NULL) {
		bapi_set_last_error("music file has no decodable audio stream");
		return -1;
	}

	music->codec = avcodec_alloc_context3(decoder);
	if (music->codec == NULL) {
		bapi_set_last_error("out of memory while creating music decoder");
		return -1;
	}
	if (avcodec_parameters_to_context(music->codec,
									  music->format->streams[music->audio_stream_index]->codecpar) < 0 ||
		avcodec_open2(music->codec, decoder, NULL) < 0) {
		bapi_set_last_error("music decoder could not be opened");
		return -1;
	}

	AVChannelLayout output_layout = AV_CHANNEL_LAYOUT_STEREO;
	if (swr_alloc_set_opts2(&music->swr, &output_layout, AV_SAMPLE_FMT_FLT, MUSIC_SAMPLE_RATE,
							&music->codec->ch_layout, music->codec->sample_fmt,
							music->codec->sample_rate, 0, NULL) < 0 ||
		swr_init(music->swr) < 0) {
		bapi_set_last_error("music resampler could not be initialized");
		return -1;
	}

	music->packet = av_packet_alloc();
	music->frame  = av_frame_alloc();
	if (music->packet == NULL || music->frame == NULL) {
		bapi_set_last_error("out of memory while creating music decode buffers");
		return -1;
	}

	if (music->format->duration > 0) {
		music->duration = (double)music->format->duration / (double)AV_TIME_BASE;
	}

	if (bapi_audio_device_handle() == NULL) {
		bapi_set_last_error("music requires a successful bapi_audio_init()");
		return -1;
	}

	const plat_interface_t *plat = plat_get();
	plat_audio_spec_t		spec = {PLAT_AUDIO_F32, MUSIC_CHANNELS, MUSIC_SAMPLE_RATE};
	music->stream				 = plat->audio.create_audio_stream(&spec);
	if (music->stream == NULL ||
		plat->audio.bind_audio_stream(bapi_audio_device_handle(), music->stream) != 0) {
		bapi_set_last_error("music audio stream could not be created");
		return -1;
	}
	music_apply_gain(music);
	return 0;
}

bapi_music_t bapi_music_load(const char *filepath)
{
	bapi_music_t music = music_create();
	if (music == NULL) return NULL;
	if (music_open(music, filepath, NULL, 0) != 0) {
		music_remove_allocated(music);
		music_close(music);
		free(music);
		return NULL;
	}
	return music;
}

bapi_music_t bapi_music_load_from_memory(const void *data, size_t size)
{
	if (data == NULL || size == 0) {
		bapi_set_last_error("music data is empty");
		return NULL;
	}
	bapi_music_t music = music_create();
	if (music == NULL) return NULL;
	if (music_open(music, NULL, data, size) != 0) {
		music_remove_allocated(music);
		music_close(music);
		free(music);
		return NULL;
	}
	return music;
}

void bapi_music_free(bapi_music_t music)
{
	if (music == NULL) return;
	music_remove_allocated(music);
	music_close(music);
	free(music);
}

int bapi_music_play(bapi_music_t music)
{
	if (music == NULL || music->stream == NULL) return -1;
	const plat_interface_t *plat = plat_get();
	plat->audio.clear_audio_stream(music->stream);
	if (plat->audio.resume_audio_stream_device != NULL) {
		plat->audio.resume_audio_stream_device(music->stream);
	}
	music_seek_start(music);
	music->playing = 1;
	music->paused  = 0;
	music->eof	   = 0;
	music_decode_some(music);
	return 0;
}

void bapi_music_pause(bapi_music_t music)
{
	if (music == NULL || music->stream == NULL) return;
	music->paused = 1;
	const plat_interface_t *plat = plat_get();
	if (plat->audio.pause_audio_stream_device != NULL) {
		plat->audio.pause_audio_stream_device(music->stream);
	}
}

void bapi_music_resume(bapi_music_t music)
{
	if (music == NULL || music->stream == NULL) return;
	music->paused = 0;
	const plat_interface_t *plat = plat_get();
	if (plat->audio.resume_audio_stream_device != NULL) {
		plat->audio.resume_audio_stream_device(music->stream);
	}
}

void bapi_music_stop(bapi_music_t music)
{
	if (music == NULL || music->stream == NULL) return;
	const plat_interface_t *plat = plat_get();
	plat->audio.clear_audio_stream(music->stream);
	if (plat->audio.pause_audio_stream_device != NULL) {
		plat->audio.pause_audio_stream_device(music->stream);
	}
	music_seek_start(music);
	music->playing = 0;
	music->paused  = 0;
	music->eof	   = 0;
}

void bapi_music_set_volume(bapi_music_t music, float volume)
{
	if (music == NULL) return;
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	music->volume = volume;
	music_apply_gain(music);
}

void bapi_music_set_loop(bapi_music_t music, int loop)
{
	if (music != NULL) music->loop = loop != 0;
}

int bapi_music_is_playing(bapi_music_t music)
{
	return music != NULL && music->playing && !music->paused;
}

double bapi_music_get_duration(bapi_music_t music)
{
	return music != NULL ? music->duration : 0.0;
}

void bapi_music_update(void)
{
	bapi_music_t music = g_allocated_music;
	while (music != NULL) {
		bapi_music_t next = music->next_allocated;
		if (music->playing && !music->paused && !music->eof) {
			music_decode_some(music);
		}
		if (music->playing && music->eof) {
			const plat_interface_t *plat = plat_get();
			int queued = music->stream != NULL ? plat->audio.get_audio_stream_queued(music->stream)
											   : 0;
			if (queued <= 0) music->playing = 0;
		}
		music = next;
	}
}

void bapi_music_apply_master_volume(float master)
{
	(void)master;
	for (bapi_music_t music = g_allocated_music; music != NULL; music = music->next_allocated) {
		music_apply_gain(music);
	}
}

void bapi_music_cleanup(void)
{
	while (g_allocated_music != NULL) {
		bapi_music_free(g_allocated_music);
	}
}
