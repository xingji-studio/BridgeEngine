#include "BridgeEngine.h"
#include "internal/bapi_internal.h"
#include "internal/bapi_internal.h"
#include "internal/platform/platform.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_AUDIO_BYTES_PER_SAMPLE ((int)sizeof(float) * 2)

/* Engine-level byte source handed to the FFmpeg AVIO callbacks. Three kinds:
 * disk files (plat_io_t), owned memory copies, and pack entry streams. */
typedef struct {
	void   *user;
	size_t (*read)(void *user, void *buf, size_t size);
	int64_t (*seek)(void *user, int64_t offset, int whence);
	int64_t (*size)(void *user);
	void   (*close)(void *user);
} video_source_t;

struct video_mem_source {
	uint8_t *data;
	size_t	 size;
	size_t	 pos;
};

struct bapi_video_internal {
	AVFormatContext	  *format_ctx;
	AVCodecContext	  *codec_ctx;
	AVStream		  *video_stream;
	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
	AVCodecContext	  *audio_ctx;
	AVFrame			  *frame;
	AVFrame			  *frame_rgb;
	AVPacket		  *packet;
	uint8_t			  *buffer;
	int				   buffer_size;

	plat_texture_t		texture;
	plat_audio_stream_t audio_stream;
	AVFrame			   *audio_frame;
	uint8_t			   *audio_buffer;
	int					audio_buffer_size;

	video_source_t		source;
	uint8_t				*avio_buffer;

	int				   video_stream_idx;
	int				   audio_stream_idx;
	int				   source_width;
	int				   source_height;
	enum AVPixelFormat source_pix_fmt;
	int				   width;
	int				   height;
	double			   fps;
	double			   time_base;

	int	   playing;
	int	   paused;
	int	   loop;
	float  volume;
	double current_time;
	double duration;
	int	   demux_eof;

	char	*filepath;
	uint32_t last_update;
	struct bapi_video_internal *next_allocated;
};

static bapi_video_t g_current_video = NULL;
static bapi_video_t g_allocated_videos = NULL;
static int		  video_unsupported_warning_logged;

static void warn_video_unsupported_once(const plat_interface_t *plat)
{
	if (!video_unsupported_warning_logged && plat != NULL && plat->core.log_warn != NULL) {
		plat->core.log_warn("Video is not supported by this platform");
		video_unsupported_warning_logged = 1;
	}
}

static size_t video_disk_read(void *user, void *buf, size_t size)
{
	plat_io_t *io = (plat_io_t *)user;
	const plat_interface_t *plat = plat_get();
	if (!plat || !io || !buf) return 0;
	return plat->io.read(io, buf, size);
}

static int64_t video_disk_seek(void *user, int64_t offset, int whence)
{
	plat_io_t *io = (plat_io_t *)user;
	const plat_interface_t *plat = plat_get();
	if (!plat || !io) return -1;
	return plat->io.seek(io, offset, whence);
}

static int64_t video_disk_size(void *user)
{
	plat_io_t *io = (plat_io_t *)user;
	const plat_interface_t *plat = plat_get();
	if (!plat || !io) return -1;
	return plat->io.size(io);
}

static void video_disk_close(void *user)
{
	plat_io_t *io = (plat_io_t *)user;
	const plat_interface_t *plat = plat_get();
	if (plat && io) plat->io.close(io);
}

static size_t video_mem_read(void *user, void *buf, size_t size)
{
	struct video_mem_source *mem = (struct video_mem_source *)user;
	if (!mem || !buf) return 0;
	size_t remaining = mem->size - mem->pos;
	size_t want	  = remaining < size ? remaining : size;
	memcpy(buf, mem->data + mem->pos, want);
	mem->pos += want;
	return want;
}

static int64_t video_mem_seek(void *user, int64_t offset, int whence)
{
	struct video_mem_source *mem = (struct video_mem_source *)user;
	if (!mem) return -1;

	uint64_t base;
	if (whence == SEEK_SET) {
		base = 0;
	} else if (whence == SEEK_CUR) {
		base = mem->pos;
	} else if (whence == SEEK_END) {
		base = mem->size;
	} else {
		return -1;
	}

	uint64_t target;
	if (offset < 0) {
		uint64_t magnitude = (uint64_t)(-(offset + 1)) + 1;
		target			   = magnitude <= base ? base - magnitude : 0;
	} else if (base > UINT64_MAX - (uint64_t)offset) {
		target = mem->size; /* overflow: clamp to end */
	} else {
		target = base + (uint64_t)offset;
		if (target > mem->size) target = mem->size;
	}

	mem->pos = (size_t)target;
	return (int64_t)target;
}

static int64_t video_mem_size(void *user)
{
	struct video_mem_source *mem = (struct video_mem_source *)user;
	return mem ? (int64_t)mem->size : -1;
}

static void video_mem_close(void *user)
{
	struct video_mem_source *mem = (struct video_mem_source *)user;
	if (mem) {
		free(mem->data);
		free(mem);
	}
}

static size_t video_pack_read(void *user, void *buf, size_t size)
{
	return bapi_pack_stream_read((bapi_pack_stream_t)user, buf, size);
}

static int64_t video_pack_seek(void *user, int64_t offset, int whence)
{
	return bapi_pack_stream_seek((bapi_pack_stream_t)user, offset, whence);
}

static int64_t video_pack_size(void *user)
{
	return bapi_pack_stream_size((bapi_pack_stream_t)user);
}

static void video_pack_close(void *user)
{
	bapi_pack_stream_close((bapi_pack_stream_t)user);
}

static int video_io_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	video_source_t *source = (video_source_t *)opaque;
	if (!source || !source->user || !source->read || !buf || buf_size <= 0) return AVERROR(EIO);
	size_t bytes_read = source->read(source->user, buf, (size_t)buf_size);
	return bytes_read > 0 ? (int)bytes_read : AVERROR_EOF;
}

static int64_t video_io_seek_packet(void *opaque, int64_t offset, int whence)
{
	video_source_t *source = (video_source_t *)opaque;
	if (!source || !source->user || !source->seek || !source->size) return -1;
	if (whence == AVSEEK_SIZE) {
		return source->size(source->user);
	}
	return source->seek(source->user, offset, whence);
}

static void remove_allocated_video(bapi_video_t video)
{
	bapi_video_t *current = &g_allocated_videos;
	while (*current != NULL) {
		if (*current == video) {
			*current = video->next_allocated;
			return;
		}
		current = &(*current)->next_allocated;
	}
}

static char *bapi_video_copy_string(const char *text)
{
	size_t len	= strlen(text) + 1;
	char  *copy = malloc(len);
	if (copy != NULL) {
		memcpy(copy, text, len);
	}
	return copy;
}

static void reset_audio_playback(bapi_video_t video)
{
	const plat_interface_t *plat = plat_get();
	if (video->audio_stream) {
		plat->audio.clear_audio_stream(video->audio_stream);
	}
	if (video->audio_ctx) {
		avcodec_flush_buffers(video->audio_ctx);
	}
	if (video->swr_ctx) {
		swr_close(video->swr_ctx);
		swr_init(video->swr_ctx);
	}
}

static void reset_video_playback_state(bapi_video_t video)
{
	video->demux_eof = 0;
	avcodec_flush_buffers(video->codec_ctx);
	if (video->audio_ctx) {
		avcodec_flush_buffers(video->audio_ctx);
	}
	reset_audio_playback(video);
}

static int ensure_video_output(bapi_video_t video, int width, int height)
{
	if (width <= 0 || height <= 0) {
		return -1;
	}

	const plat_interface_t *plat = plat_get();

	if (video->texture != NULL && video->buffer != NULL && video->width == width &&
		video->height == height) {
		return 0;
	}

	int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_BGRA, width, height, 1);
	if (buffer_size < 0) {
		return -1;
	}

	uint8_t *buffer = av_malloc((size_t)buffer_size);
	if (buffer == NULL) {
		return -1;
	}

	int fill_result = av_image_fill_arrays(video->frame_rgb->data, video->frame_rgb->linesize,
										   buffer, AV_PIX_FMT_BGRA, width, height, 1);
	if (fill_result < 0) {
		av_free(buffer);
		return -1;
	}

	plat_texture_t texture = video->texture;
	if (texture == NULL || video->width != width || video->height != height) {
		texture =
			plat->texture.create_texture(bapi_internal_get_renderer(), PLAT_PIXELFORMAT_ARGB8888,
										 PLAT_TEXTUREACCESS_STREAMING, width, height);
		if (!texture) {
			av_free(buffer);
			return -1;
		}
	}

	if (texture != video->texture && video->texture != NULL) {
		plat->texture.destroy_texture(video->texture);
	}
	if (video->buffer != NULL) {
		av_free(video->buffer);
	}

	video->buffer			 = buffer;
	video->buffer_size		 = buffer_size;
	video->texture			 = texture;
	video->width			 = width;
	video->height			 = height;
	video->frame_rgb->format = AV_PIX_FMT_BGRA;
	video->frame_rgb->width	 = width;
	video->frame_rgb->height = height;

	return 0;
}

static int ensure_sws_context(bapi_video_t video, const AVFrame *frame)
{
	enum AVPixelFormat frame_format = (enum AVPixelFormat)frame->format;

	if (frame_format == AV_PIX_FMT_NONE || frame->width <= 0 || frame->height <= 0) {
		return -1;
	}

	if (video->sws_ctx != NULL && video->source_width == frame->width &&
		video->source_height == frame->height && video->source_pix_fmt == frame_format) {
		return 0;
	}

	if (video->sws_ctx != NULL) {
		sws_freeContext(video->sws_ctx);
		video->sws_ctx = NULL;
	}

	video->sws_ctx = sws_getContext(frame->width, frame->height, frame_format, frame->width,
									frame->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
	if (video->sws_ctx == NULL) {
		return -1;
	}

	video->source_width	  = frame->width;
	video->source_height  = frame->height;
	video->source_pix_fmt = frame_format;
	return 0;
}

static int present_video_frame(bapi_video_t video, AVFrame *frame)
{
	const plat_interface_t *plat = plat_get();

	if (ensure_video_output(video, frame->width, frame->height) < 0) {
		printf("[VIDEO] Error: Failed to prepare output surface\n");
		return -1;
	}

	if (ensure_sws_context(video, frame) < 0) {
		printf("[VIDEO] Error: Failed to create sws context\n");
		return -1;
	}

	int scaled_height =
		sws_scale(video->sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0,
				  frame->height, video->frame_rgb->data, video->frame_rgb->linesize);
	if (scaled_height <= 0) {
		printf("[VIDEO] Error: Failed to scale video frame\n");
		return -1;
	}

	int64_t timestamp = frame->best_effort_timestamp;
	if (timestamp == AV_NOPTS_VALUE) {
		timestamp = frame->pts;
	}
	if (timestamp != AV_NOPTS_VALUE) {
		video->current_time = (double)timestamp * video->time_base;
	}

	if (plat->texture.update_texture(video->texture, video->frame_rgb->data[0],
									 video->frame_rgb->linesize[0]) != 0) {
		printf("[VIDEO] Error: Failed to update texture\n");
		return -1;
	}

	return 0;
}

static int queue_audio_frame(bapi_video_t video, AVFrame *frame)
{
	const plat_interface_t *plat = plat_get();

	int64_t out_samples64 = av_rescale_rnd(
		swr_get_delay(video->swr_ctx, video->audio_ctx->sample_rate) + frame->nb_samples, 44100,
		video->audio_ctx->sample_rate, AV_ROUND_UP);
	if (out_samples64 > INT_MAX) {
		printf("[VIDEO] Audio frame is too large\n");
		return -1;
	}
	int out_samples	  = (int)out_samples64;
	int required_size = av_samples_get_buffer_size(NULL, 2, out_samples, AV_SAMPLE_FMT_FLT, 0);
	if (required_size < 0) {
		printf("[VIDEO] Failed to compute audio buffer size\n");
		return -1;
	}

	if (required_size > video->audio_buffer_size) {
		uint8_t *resized = av_realloc(video->audio_buffer, required_size);
		if (resized == NULL) {
			printf("[VIDEO] Failed to allocate audio buffer\n");
			return -1;
		}
		video->audio_buffer		 = resized;
		video->audio_buffer_size = required_size;
	}

	uint8_t *output[] = {video->audio_buffer, NULL};
	int		 converted_samples =
		swr_convert(video->swr_ctx, output, out_samples,
					(const uint8_t *const *)frame->extended_data, frame->nb_samples);
	if (converted_samples < 0) {
		printf("[VIDEO] Failed to resample audio frame\n");
		return -1;
	}

	int output_size = converted_samples * VIDEO_AUDIO_BYTES_PER_SAMPLE;
	if (output_size <= 0) {
		return 0;
	}

	if (video->volume < 0.99f) {
		float *samples		= (float *)video->audio_buffer;
		int	   sample_count = output_size / (int)sizeof(float);
		for (int i = 0; i < sample_count; ++i) {
			samples[i] *= video->volume;
		}
	}

	if (plat->audio.put_audio_stream_data(video->audio_stream, video->audio_buffer, output_size) !=
		0) {
		printf("[VIDEO] Failed to queue audio data\n");
		return -1;
	}

	if (plat->audio.flush_audio_stream(video->audio_stream) != 0) {
		printf("[VIDEO] Failed to flush audio stream\n");
		return -1;
	}

	return 0;
}

static int decode_audio_packet(bapi_video_t video)
{
	int ret = avcodec_send_packet(video->audio_ctx, video->packet);
	if (ret < 0) {
		return ret;
	}

	while ((ret = avcodec_receive_frame(video->audio_ctx, video->audio_frame)) >= 0) {
		if (queue_audio_frame(video, video->audio_frame) < 0) {
			av_frame_unref(video->audio_frame);
			return -1;
		}
		av_frame_unref(video->audio_frame);
	}

	return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

static int init_audio_decoder(bapi_video_t video)
{
	const plat_interface_t *plat = plat_get();

	if (video->audio_stream_idx < 0) {
		return 0;
	}

	const AVCodec *audio_codec = avcodec_find_decoder(
		video->format_ctx->streams[video->audio_stream_idx]->codecpar->codec_id);
	if (!audio_codec) {
		printf("[VIDEO] Audio codec not found\n");
		return -1;
	}

	video->audio_ctx = avcodec_alloc_context3(audio_codec);
	if (!video->audio_ctx) {
		return -1;
	}

	if (avcodec_parameters_to_context(
			video->audio_ctx, video->format_ctx->streams[video->audio_stream_idx]->codecpar) < 0) {
		avcodec_free_context(&video->audio_ctx);
		return -1;
	}

	if (avcodec_open2(video->audio_ctx, audio_codec, NULL) < 0) {
		avcodec_free_context(&video->audio_ctx);
		return -1;
	}

	AVChannelLayout input_layout  = {0};
	AVChannelLayout output_layout = AV_CHANNEL_LAYOUT_STEREO;

	if (av_channel_layout_copy(&input_layout, &video->audio_ctx->ch_layout) < 0 ||
		input_layout.nb_channels <= 0) {
		av_channel_layout_uninit(&input_layout);
		av_channel_layout_default(&input_layout, 2);
	}

	if (swr_alloc_set_opts2(&video->swr_ctx, &output_layout, AV_SAMPLE_FMT_FLT, 44100,
							&input_layout, video->audio_ctx->sample_fmt,
							video->audio_ctx->sample_rate, 0, NULL) < 0) {
		av_channel_layout_uninit(&input_layout);
		avcodec_free_context(&video->audio_ctx);
		return -1;
	}

	av_channel_layout_uninit(&input_layout);

	if (!video->swr_ctx || swr_init(video->swr_ctx) < 0) {
		swr_free(&video->swr_ctx);
		avcodec_free_context(&video->audio_ctx);
		return -1;
	}

	video->audio_stream = plat->audio.open_audio_device_stream(PLAT_AUDIO_F32, 2, 44100);
	if (!video->audio_stream) {
		swr_free(&video->swr_ctx);
		avcodec_free_context(&video->audio_ctx);
		printf("[VIDEO] Failed to open audio device stream\n");
		return -1;
	}

	video->audio_frame = av_frame_alloc();
	if (!video->audio_frame) {
		plat->audio.destroy_audio_stream(video->audio_stream);
		video->audio_stream = NULL;
		swr_free(&video->swr_ctx);
		avcodec_free_context(&video->audio_ctx);
		return -1;
	}

	plat->audio.resume_audio_stream_device(video->audio_stream);

	return 0;
}

int bapi_video_init(void)
{
	const plat_interface_t *plat = plat_get();
	if (!plat_supports(PLAT_CAPABILITY_VIDEO)) {
		warn_video_unsupported_once(plat);
		return 1;
	}

	printf("[VIDEO] Video subsystem initialized (FFmpeg version)\n");
	return 0;
}

void bapi_video_cleanup(void)
{
	while (g_allocated_videos != NULL) {
		bapi_video_free(g_allocated_videos);
	}
	printf("[VIDEO] Video subsystem cleaned up\n");
}

static bapi_video_t bapi_video_load_with_source(video_source_t source, const char *label)
{
	bapi_video_t video = malloc(sizeof(struct bapi_video_internal));
	if (video == NULL) {
		printf("[VIDEO] Error: Failed to allocate video struct\n");
		if (source.user && source.close) source.close(source.user);
		return NULL;
	}
	memset(video, 0, sizeof(struct bapi_video_internal));

	video->filepath = bapi_video_copy_string(label);
	if (video->filepath == NULL) {
		printf("[VIDEO] Error: Failed to copy filepath\n");
		if (source.user && source.close) source.close(source.user);
		free(video);
		return NULL;
	}
	video->volume			= 1.0f;
	video->loop				= 0;
	video->playing			= 0;
	video->paused			= 0;
	video->video_stream_idx = -1;
	video->audio_stream_idx = -1;
	video->source_pix_fmt	= AV_PIX_FMT_NONE;
	video->source			= source;
	video->avio_buffer		= NULL;

	if (source.user != NULL) {
		uint8_t *avio_buf = (uint8_t *)av_malloc(4096);
		if (!avio_buf) {
			goto load_error;
		}
		video->avio_buffer = avio_buf;

		AVIOContext *avio = avio_alloc_context(avio_buf, 4096, 0, &video->source,
			video_io_read_packet, NULL, video_io_seek_packet);
		if (!avio) {
			av_free(avio_buf);
			video->avio_buffer = NULL;
			goto load_error;
		}

		video->format_ctx = avformat_alloc_context();
		if (!video->format_ctx) {
			avio_context_free(&avio);
			video->avio_buffer = NULL;
			goto load_error;
		}
		video->format_ctx->pb = avio;

		if (avformat_open_input(&video->format_ctx, "", NULL, NULL) != 0) {
			printf("[VIDEO] Error: Cannot open video source %s\n", label);
			goto load_error;
		}
	} else {
		video->format_ctx = avformat_alloc_context();
		if (avformat_open_input(&video->format_ctx, label, NULL, NULL) != 0) {
			printf("[VIDEO] Error: Cannot open video file %s\n", label);
			avformat_free_context(video->format_ctx);
			video->format_ctx = NULL;
			goto load_error;
		}
	}

	if (avformat_find_stream_info(video->format_ctx, NULL) < 0) {
		printf("[VIDEO] Error: Cannot find stream info\n");
		goto load_error;
	}

	for (unsigned int i = 0; i < video->format_ctx->nb_streams; i++) {
		if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
			video->video_stream_idx < 0) {
			video->video_stream_idx = (int)i;
		}
		if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
			video->audio_stream_idx < 0) {
			video->audio_stream_idx = (int)i;
		}
	}

	if (video->video_stream_idx < 0) {
		printf("[VIDEO] Error: No video stream found\n");
		goto load_error;
	}

	video->video_stream	 = video->format_ctx->streams[video->video_stream_idx];
	const AVCodec *codec = avcodec_find_decoder(video->video_stream->codecpar->codec_id);
	if (!codec) {
		printf("[VIDEO] Error: Codec not found\n");
		goto load_error;
	}

	video->codec_ctx = avcodec_alloc_context3(codec);
	if (!video->codec_ctx) {
		printf("[VIDEO] Error: Failed to allocate codec context\n");
		goto load_error;
	}

	if (avcodec_parameters_to_context(video->codec_ctx, video->video_stream->codecpar) < 0) {
		printf("[VIDEO] Error: Failed to copy codec parameters\n");
		goto load_error;
	}

	if (avcodec_open2(video->codec_ctx, codec, NULL) < 0) {
		printf("[VIDEO] Error: Failed to open codec\n");
		goto load_error;
	}

	video->time_base = av_q2d(video->video_stream->time_base);
	video->duration	 = (double)video->format_ctx->duration / AV_TIME_BASE;

	AVRational frame_rate = av_guess_frame_rate(video->format_ctx, video->video_stream, NULL);
	if (frame_rate.num > 0 && frame_rate.den > 0) {
		video->fps = av_q2d(frame_rate);
	} else {
		video->fps = 30.0;
	}

	video->frame	 = av_frame_alloc();
	video->frame_rgb = av_frame_alloc();
	if (!video->frame || !video->frame_rgb) {
		printf("[VIDEO] Error: Failed to allocate frames\n");
		goto load_error;
	}

	video->packet = av_packet_alloc();
	if (!video->packet) {
		printf("[VIDEO] Error: Failed to allocate packet\n");
		goto load_error;
	}

	if (init_audio_decoder(video) < 0) {
		printf("[VIDEO] Warning: audio track disabled for %s\n", label);
	}

	printf("[VIDEO] Loaded: %s (%dx%d @ %.2f fps, duration: %.2fs)\n", label, video->width,
		   video->height, video->fps, video->duration);

	video->next_allocated = g_allocated_videos;
	g_allocated_videos = video;
	return video;

load_error:
	if (video->sws_ctx) sws_freeContext(video->sws_ctx);
	if (video->buffer) av_free(video->buffer);
	if (video->frame_rgb) av_frame_free(&video->frame_rgb);
	if (video->frame) av_frame_free(&video->frame);
	if (video->codec_ctx) avcodec_free_context(&video->codec_ctx);
	if (video->format_ctx) {
		avformat_close_input(&video->format_ctx);
		video->avio_buffer = NULL; /* freed together with the AVIOContext */
	}
	if (video->avio_buffer) {
		av_free(video->avio_buffer);
		video->avio_buffer = NULL;
	}
	if (video->source.user && video->source.close) {
		video->source.close(video->source.user);
		video->source.user = NULL;
	}
	if (video->filepath) free(video->filepath);
	free(video);
	return NULL;
}

static bapi_video_t video_load_guarded(video_source_t source, const char *label)
{
	const plat_interface_t *plat = plat_get();
	if (!plat_supports(PLAT_CAPABILITY_VIDEO)) {
		warn_video_unsupported_once(plat);
		if (source.user && source.close) source.close(source.user);
		return NULL;
	}

	if (bapi_internal_get_renderer() == NULL) {
		printf("[VIDEO] Error: renderer not initialized\n");
		if (source.user && source.close) source.close(source.user);
		return NULL;
	}

	return bapi_video_load_with_source(source, label);
}

bapi_video_t bapi_video_load(const char *filepath)
{
	const plat_interface_t *plat = plat_get();
	if (!plat_supports(PLAT_CAPABILITY_VIDEO)) {
		warn_video_unsupported_once(plat);
		return NULL;
	}

	if (filepath == NULL) {
		printf("[VIDEO] Error: filepath is NULL\n");
		bapi_set_last_error("video filepath is NULL");
		return NULL;
	}

	video_source_t source = {NULL, NULL, NULL, NULL, NULL};
	if (plat && plat->io.open_read) {
		plat_io_t *io = plat->io.open_read(filepath);
		if (!io) {
			printf("[VIDEO] Error: Cannot open file %s\n", filepath);
			bapi_set_last_error("video file could not be opened: %s", filepath);
			return NULL;
		}
		source.user  = io;
		source.read  = video_disk_read;
		source.seek  = video_disk_seek;
		source.size  = video_disk_size;
		source.close = video_disk_close;
	}

	return video_load_guarded(source, filepath);
}

bapi_video_t bapi_video_load_from_memory(const void *data, size_t size)
{
	if (!data || size == 0) {
		printf("[VIDEO] Error: invalid memory source\n");
		return NULL;
	}

	struct video_mem_source *mem =
		(struct video_mem_source *)malloc(sizeof(struct video_mem_source));
	if (!mem) return NULL;

	mem->data = (uint8_t *)malloc(size > 0 ? size : 1);
	if (!mem->data) {
		free(mem);
		return NULL;
	}
	memcpy(mem->data, data, size);
	mem->size = size;
	mem->pos  = 0;

	video_source_t source;
	source.user  = mem;
	source.read  = video_mem_read;
	source.seek  = video_mem_seek;
	source.size  = video_mem_size;
	source.close = video_mem_close;

	return video_load_guarded(source, "<memory>");
}

bapi_video_t bapi_video_load_from_pack_stream(bapi_pack_t pack, const char *name)
{
	if (!name) {
		printf("[VIDEO] Error: pack entry name is NULL\n");
		return NULL;
	}

	bapi_pack_stream_t stream = bapi_pack_stream_open(pack, name);
	if (!stream) {
		printf("[VIDEO] Error: Cannot open pack entry %s\n", name);
		return NULL;
	}

	video_source_t source;
	source.user  = stream;
	source.read  = video_pack_read;
	source.seek  = video_pack_seek;
	source.size  = video_pack_size;
	source.close = video_pack_close;

	size_t label_length = strlen(name) + 6; /* "pack:" + name + NUL */
	char  *label		   = (char *)malloc(label_length);
	if (!label) {
		bapi_pack_stream_close(stream);
		return NULL;
	}
	snprintf(label, label_length, "pack:%s", name);

	bapi_video_t video = video_load_guarded(source, label);
	free(label);
	return video;
}

void bapi_video_free(bapi_video_t video)
{
	if (video == NULL) {
		return;
	}

	const plat_interface_t *plat = plat_get();

	if (g_current_video == video) {
		g_current_video = NULL;
	}
	remove_allocated_video(video);

	if (video->audio_stream) {
		plat->audio.destroy_audio_stream(video->audio_stream);
	}
	if (video->audio_frame) {
		av_frame_free(&video->audio_frame);
	}
	if (video->swr_ctx) {
		swr_free(&video->swr_ctx);
	}
	if (video->packet) {
		av_packet_free(&video->packet);
	}
	if (video->texture) {
		plat->texture.destroy_texture(video->texture);
	}
	if (video->sws_ctx) {
		sws_freeContext(video->sws_ctx);
	}
	if (video->buffer) {
		av_free(video->buffer);
	}
	if (video->frame_rgb) {
		av_frame_free(&video->frame_rgb);
	}
	if (video->frame) {
		av_frame_free(&video->frame);
	}
	if (video->codec_ctx) {
		avcodec_free_context(&video->codec_ctx);
	}
	if (video->format_ctx) {
		avformat_close_input(&video->format_ctx);
		video->avio_buffer = NULL;
	}
	if (video->source.user && video->source.close) {
		video->source.close(video->source.user);
		video->source.user = NULL;
	}
	if (video->avio_buffer) {
		av_free(video->avio_buffer);
		video->avio_buffer = NULL;
	}
	if (video->filepath) {
		free(video->filepath);
	}
	if (video->audio_ctx) {
		avcodec_free_context(&video->audio_ctx);
	}
	if (video->audio_buffer) {
		av_free(video->audio_buffer);
	}

	free(video);
}

static int decode_video_frame(bapi_video_t video)
{
	for (;;) {
		int ret = avcodec_receive_frame(video->codec_ctx, video->frame);
		if (ret == 0) {
			int present_result = present_video_frame(video, video->frame);
			av_frame_unref(video->frame);
			return present_result;
		}
		if (ret != AVERROR(EAGAIN)) {
			return ret == AVERROR_EOF ? -1 : ret;
		}

		if (!video->demux_eof) {
			ret = av_read_frame(video->format_ctx, video->packet);
			if (ret < 0) {
				video->demux_eof = 1;
				avcodec_send_packet(video->codec_ctx, NULL);
				if (video->audio_ctx) {
					avcodec_send_packet(video->audio_ctx, NULL);
				}
				continue;
			}

			if (video->packet->stream_index == video->video_stream_idx) {
				ret = avcodec_send_packet(video->codec_ctx, video->packet);
				av_packet_unref(video->packet);
				if (ret < 0 && ret != AVERROR(EAGAIN)) {
					return ret;
				}
				continue;
			}

			if (video->audio_ctx != NULL &&
				video->packet->stream_index == video->audio_stream_idx) {
				ret = decode_audio_packet(video);
				av_packet_unref(video->packet);
				if (ret < 0) {
					return ret;
				}
				continue;
			}

			av_packet_unref(video->packet);
			continue;
		}

		return -1;
	}
}

int bapi_video_play(bapi_video_t video)
{
	if (video == NULL) {
		return 1;
	}

	const plat_interface_t *plat = plat_get();

	if (!video->playing) {
		av_seek_frame(video->format_ctx, video->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
		reset_video_playback_state(video);
		video->current_time = 0;
	}

	video->playing	   = 1;
	video->paused	   = 0;
	video->last_update = plat->core.get_ticks();
	g_current_video	   = video;

	printf("[VIDEO] Playing: %s\n", video->filepath);
	return 0;
}

void bapi_video_pause(bapi_video_t video)
{
	if (video == NULL) {
		return;
	}

	video->paused = !video->paused;
	printf("[VIDEO] %s: %s\n", video->paused ? "Paused" : "Resumed", video->filepath);
}

void bapi_video_stop(bapi_video_t video)
{
	if (video == NULL) {
		return;
	}

	const plat_interface_t *plat = plat_get();

	video->playing		= 0;
	video->paused		= 0;
	video->current_time = 0;

	if (video->audio_stream) {
		plat->audio.clear_audio_stream(video->audio_stream);
	}
	if (video->audio_ctx) {
		avcodec_flush_buffers(video->audio_ctx);
	}
	video->demux_eof = 0;

	if (g_current_video == video) {
		g_current_video = NULL;
	}

	printf("[VIDEO] Stopped: %s\n", video->filepath);
}

void bapi_video_render(bapi_video_t video, int x, int y, int w, int h)
{
	plat_renderer_t renderer = bapi_internal_get_renderer();
	if (video == NULL || video->texture == NULL || renderer == NULL) {
		return;
	}

	const plat_interface_t *plat = plat_get();
	plat->renderer.render_texture(renderer, video->texture, (float)x, (float)y, (float)w, (float)h);
}

void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h)
{
	plat_renderer_t renderer = bapi_internal_get_renderer();
	if (video == NULL || video->texture == NULL || renderer == NULL) {
		return;
	}

	const plat_interface_t *plat = plat_get();

	float video_aspect = (float)video->width / (float)video->height;
	float area_aspect  = (float)area_w / (float)area_h;

	int render_w, render_h, render_x, render_y;

	if (video_aspect > area_aspect) {
		render_w = area_w;
		render_h = (int)((float)area_w / video_aspect);
		render_x = area_x;
		render_y = area_y + (area_h - render_h) / 2;
	} else {
		render_h = area_h;
		render_w = (int)((float)area_h * video_aspect);
		render_x = area_x + (area_w - render_w) / 2;
		render_y = area_y;
	}

	plat->renderer.render_texture(renderer, video->texture, (float)render_x, (float)render_y,
								  (float)render_w, (float)render_h);
}

void bapi_video_render_center(bapi_video_t video, int window_w, int window_h)
{
	plat_renderer_t renderer = bapi_internal_get_renderer();
	if (video == NULL || video->texture == NULL || renderer == NULL) {
		return;
	}

	const plat_interface_t *plat = plat_get();

	float video_aspect	= (float)video->width / (float)video->height;
	float window_aspect = (float)window_w / (float)window_h;

	int render_w, render_h, render_x, render_y;

	if (video_aspect > window_aspect) {
		render_w = window_w;
		render_h = (int)((float)window_w / video_aspect);
		render_x = 0;
		render_y = (window_h - render_h) / 2;
	} else {
		render_h = window_h;
		render_w = (int)((float)window_h * video_aspect);
		render_x = (window_w - render_w) / 2;
		render_y = 0;
	}

	plat->renderer.render_texture(renderer, video->texture, (float)render_x, (float)render_y,
								  (float)render_w, (float)render_h);
}

void bapi_video_set_loop(bapi_video_t video, int loop)
{
	if (video != NULL) {
		video->loop = loop;
	}
}

void bapi_video_set_volume(bapi_video_t video, float volume)
{
	if (video != NULL) {
		if (volume < 0.0f) {
			video->volume = 0.0f;
		} else if (volume > 1.0f) {
			video->volume = 1.0f;
		} else {
			video->volume = volume;
		}
	}
}

int bapi_video_is_playing(bapi_video_t video)
{
	if (video == NULL) {
		return 0;
	}
	return video->playing && !video->paused;
}

void bapi_video_get_size(bapi_video_t video, int *w, int *h)
{
	if (video == NULL) {
		if (w != NULL) *w = 0;
		if (h != NULL) *h = 0;
		return;
	}

	if (w != NULL) *w = video->width;
	if (h != NULL) *h = video->height;
}

void bapi_video_update(void)
{
	if (g_current_video == NULL || !g_current_video->playing || g_current_video->paused) {
		return;
	}

	const plat_interface_t *plat  = plat_get();
	bapi_video_t			video = g_current_video;

	uint32_t now		 = plat->core.get_ticks();
	uint32_t frame_delay = (uint32_t)(1000.0 / video->fps);

	if (now - video->last_update < frame_delay) {
		return;
	}

	video->last_update = now;

	if (decode_video_frame(video) < 0) {
		if (video->loop) {
			av_seek_frame(video->format_ctx, video->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
			reset_video_playback_state(video);
			video->current_time = 0;
			decode_video_frame(video);
		} else {
			video->playing	= 0;
			g_current_video = NULL;
			printf("[VIDEO] Playback finished: %s\n", video->filepath);
		}
	}
}
