#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "video/video.h"
#include "bapi_internal.h"

extern SDL_Renderer* bapi_internal_renderer;

struct bapi_video_internal {
    AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    AVStream* video_stream;
    struct SwsContext* sws_ctx;
    struct SwrContext* swr_ctx;
    AVFrame* frame;
    AVFrame* frame_rgb;
    AVPacket* packet;
    uint8_t* buffer;
    int buffer_size;
    
    SDL_Texture* texture;
    SDL_AudioStream* audio_stream;
    
    int video_stream_idx;
    int audio_stream_idx;
    int width;
    int height;
    double fps;
    double time_base;
    
    int playing;
    int paused;
    int loop;
    float volume;
    double current_time;
    double duration;
    
    char* filepath;
    Uint32 last_update;
};

static bapi_video_t g_current_video = NULL;

static int init_audio_decoder(bapi_video_t video) {
    if (video->audio_stream_idx < 0) {
        return 0;
    }
    
    const AVCodec* audio_codec = avcodec_find_decoder(video->format_ctx->streams[video->audio_stream_idx]->codecpar->codec_id);
    if (!audio_codec) {
        printf("[VIDEO] Audio codec not found\n");
        return -1;
    }
    
    AVCodecContext* audio_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_ctx) {
        return -1;
    }
    
    if (avcodec_parameters_to_context(audio_ctx, video->format_ctx->streams[video->audio_stream_idx]->codecpar) < 0) {
        avcodec_free_context(&audio_ctx);
        return -1;
    }
    
    if (avcodec_open2(audio_ctx, audio_codec, NULL) < 0) {
        avcodec_free_context(&audio_ctx);
        return -1;
    }
    
    video->swr_ctx = swr_alloc();
    if (!video->swr_ctx) {
        avcodec_free_context(&audio_ctx);
        return -1;
    }
    
    av_opt_set_int(video->swr_ctx, "in_channel_layout", audio_ctx->ch_layout.u.mask ? audio_ctx->ch_layout.u.mask : AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(video->swr_ctx, "in_sample_rate", audio_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(video->swr_ctx, "in_sample_fmt", audio_ctx->sample_fmt, 0);
    
    av_opt_set_int(video->swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(video->swr_ctx, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(video->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    
    if (swr_init(video->swr_ctx) < 0) {
        swr_free(&video->swr_ctx);
        avcodec_free_context(&audio_ctx);
        return -1;
    }
    
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 44100;
    
    video->audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (video->audio_stream) {
        SDL_ResumeAudioStreamDevice(video->audio_stream);
    }
    
    avcodec_free_context(&audio_ctx);
    return 0;
}

int bapi_video_init(void) {
    printf("[VIDEO] Video subsystem initialized (FFmpeg version)\n");
    return 0;
}

void bapi_video_cleanup(void) {
    g_current_video = NULL;
    printf("[VIDEO] Video subsystem cleaned up\n");
}

bapi_video_t bapi_video_load(const char* filepath) {
    if (filepath == NULL) {
        printf("[VIDEO] Error: filepath is NULL\n");
        return NULL;
    }
    
    if (bapi_internal_renderer == NULL) {
        printf("[VIDEO] Error: renderer not initialized\n");
        return NULL;
    }
    
    bapi_video_t video = malloc(sizeof(struct bapi_video_internal));
    if (video == NULL) {
        printf("[VIDEO] Error: Failed to allocate video struct\n");
        return NULL;
    }
    memset(video, 0, sizeof(struct bapi_video_internal));
    
    video->filepath = strdup(filepath);
    video->volume = 1.0f;
    video->loop = 0;
    video->playing = 0;
    video->paused = 0;
    video->video_stream_idx = -1;
    video->audio_stream_idx = -1;
    
    if (avformat_open_input(&video->format_ctx, filepath, NULL, NULL) != 0) {
        printf("[VIDEO] Error: Cannot open video file %s\n", filepath);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    if (avformat_find_stream_info(video->format_ctx, NULL) < 0) {
        printf("[VIDEO] Error: Cannot find stream info\n");
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    for (unsigned int i = 0; i < video->format_ctx->nb_streams; i++) {
        if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video->video_stream_idx < 0) {
            video->video_stream_idx = i;
        }
        if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && video->audio_stream_idx < 0) {
            video->audio_stream_idx = i;
        }
    }
    
    if (video->video_stream_idx < 0) {
        printf("[VIDEO] Error: No video stream found\n");
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    video->video_stream = video->format_ctx->streams[video->video_stream_idx];
    const AVCodec* codec = avcodec_find_decoder(video->video_stream->codecpar->codec_id);
    if (!codec) {
        printf("[VIDEO] Error: Codec not found\n");
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    video->codec_ctx = avcodec_alloc_context3(codec);
    if (!video->codec_ctx) {
        printf("[VIDEO] Error: Failed to allocate codec context\n");
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    if (avcodec_parameters_to_context(video->codec_ctx, video->video_stream->codecpar) < 0) {
        printf("[VIDEO] Error: Failed to copy codec parameters\n");
        avcodec_free_context(&video->codec_ctx);
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    if (avcodec_open2(video->codec_ctx, codec, NULL) < 0) {
        printf("[VIDEO] Error: Failed to open codec\n");
        avcodec_free_context(&video->codec_ctx);
        avformat_close_input(&video->format_ctx);
        free(video->filepath);
        free(video);
        return NULL;
    }
    
    video->width = video->codec_ctx->width;
    video->height = video->codec_ctx->height;
    video->time_base = av_q2d(video->video_stream->time_base);
    video->duration = (double)video->format_ctx->duration / AV_TIME_BASE;
    
    if (video->video_stream->avg_frame_rate.den != 0) {
        video->fps = av_q2d(video->video_stream->avg_frame_rate);
    } else {
        video->fps = 30.0;
    }
    
    video->frame = av_frame_alloc();
    video->frame_rgb = av_frame_alloc();
    if (!video->frame || !video->frame_rgb) {
        printf("[VIDEO] Error: Failed to allocate frames\n");
        goto cleanup_error;
    }
    
    video->buffer_size = av_image_get_buffer_size(AV_PIX_FMT_BGRA, video->width, video->height, 1);
    video->buffer = av_malloc(video->buffer_size * sizeof(uint8_t));
    if (!video->buffer) {
        printf("[VIDEO] Error: Failed to allocate buffer\n");
        goto cleanup_error;
    }
    
    av_image_fill_arrays(video->frame_rgb->data, video->frame_rgb->linesize, video->buffer, AV_PIX_FMT_BGRA, video->width, video->height, 1);
    
    video->sws_ctx = sws_getContext(
        video->codec_ctx->width, video->codec_ctx->height, video->codec_ctx->pix_fmt,
        video->width, video->height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    if (!video->sws_ctx) {
        printf("[VIDEO] Error: Failed to create sws context\n");
        goto cleanup_error;
    }
    
    video->texture = SDL_CreateTexture(
        bapi_internal_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        video->width,
        video->height
    );
    if (!video->texture) {
        printf("[VIDEO] Error: Failed to create texture: %s\n", SDL_GetError());
        goto cleanup_error;
    }
    
    video->packet = av_packet_alloc();
    if (!video->packet) {
        printf("[VIDEO] Error: Failed to allocate packet\n");
        goto cleanup_error;
    }
    
    init_audio_decoder(video);
    
    printf("[VIDEO] Loaded: %s (%dx%d @ %.2f fps, duration: %.2fs)\n", 
           filepath, video->width, video->height, video->fps, video->duration);
    
    return video;
    
cleanup_error:
    if (video->sws_ctx) sws_freeContext(video->sws_ctx);
    if (video->buffer) av_free(video->buffer);
    if (video->frame_rgb) av_frame_free(&video->frame_rgb);
    if (video->frame) av_frame_free(&video->frame);
    if (video->codec_ctx) avcodec_free_context(&video->codec_ctx);
    if (video->format_ctx) avformat_close_input(&video->format_ctx);
    if (video->filepath) free(video->filepath);
    free(video);
    return NULL;
}

void bapi_video_free(bapi_video_t video) {
    if (video == NULL) {
        return;
    }
    
    if (g_current_video == video) {
        g_current_video = NULL;
    }
    
    if (video->audio_stream) {
        SDL_DestroyAudioStream(video->audio_stream);
    }
    if (video->swr_ctx) {
        swr_free(&video->swr_ctx);
    }
    if (video->packet) {
        av_packet_free(&video->packet);
    }
    if (video->texture) {
        SDL_DestroyTexture(video->texture);
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
    }
    if (video->filepath) {
        free(video->filepath);
    }
    
    free(video);
}

static int decode_video_frame(bapi_video_t video) {
    int ret;
    
    while ((ret = av_read_frame(video->format_ctx, video->packet)) >= 0) {
        if (video->packet->stream_index == video->video_stream_idx) {
            ret = avcodec_send_packet(video->codec_ctx, video->packet);
            if (ret < 0) {
                av_packet_unref(video->packet);
                continue;
            }
            
            ret = avcodec_receive_frame(video->codec_ctx, video->frame);
            if (ret == 0) {
                sws_scale(
                    video->sws_ctx,
                    (const uint8_t* const*)video->frame->data,
                    video->frame->linesize,
                    0, video->codec_ctx->height,
                    video->frame_rgb->data,
                    video->frame_rgb->linesize
                );
                
                video->current_time = video->frame->pts * video->time_base;
                
                SDL_UpdateTexture(
                    video->texture,
                    NULL,
                    video->frame_rgb->data[0],
                    video->frame_rgb->linesize[0]
                );
                
                av_packet_unref(video->packet);
                return 0;
            }
        }
        av_packet_unref(video->packet);
    }
    
    return -1;
}

int bapi_video_play(bapi_video_t video) {
    if (video == NULL) {
        return 1;
    }
    
    if (!video->playing) {
        av_seek_frame(video->format_ctx, video->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(video->codec_ctx);
        video->current_time = 0;
    }
    
    video->playing = 1;
    video->paused = 0;
    video->last_update = SDL_GetTicks();
    g_current_video = video;
    
    printf("[VIDEO] Playing: %s\n", video->filepath);
    return 0;
}

void bapi_video_pause(bapi_video_t video) {
    if (video == NULL) {
        return;
    }
    
    video->paused = !video->paused;
    printf("[VIDEO] %s: %s\n", video->paused ? "Paused" : "Resumed", video->filepath);
}

void bapi_video_stop(bapi_video_t video) {
    if (video == NULL) {
        return;
    }
    
    video->playing = 0;
    video->paused = 0;
    video->current_time = 0;
    
    if (video->audio_stream) {
        SDL_ClearAudioStream(video->audio_stream);
    }
    
    if (g_current_video == video) {
        g_current_video = NULL;
    }
    
    printf("[VIDEO] Stopped: %s\n", video->filepath);
}

void bapi_video_render(bapi_video_t video, int x, int y, int w, int h) {
    if (video == NULL || video->texture == NULL || bapi_internal_renderer == NULL) {
        return;
    }
    
    SDL_FRect destRect = {(float)x, (float)y, (float)w, (float)h};
    SDL_RenderTexture(bapi_internal_renderer, video->texture, NULL, &destRect);
}

void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h) {
    if (video == NULL || video->texture == NULL || bapi_internal_renderer == NULL) {
        return;
    }
    
    float video_aspect = (float)video->width / (float)video->height;
    float area_aspect = (float)area_w / (float)area_h;
    
    int render_w, render_h, render_x, render_y;
    
    if (video_aspect > area_aspect) {
        render_w = area_w;
        render_h = (int)(area_w / video_aspect);
        render_x = area_x;
        render_y = area_y + (area_h - render_h) / 2;
    } else {
        render_h = area_h;
        render_w = (int)(area_h * video_aspect);
        render_x = area_x + (area_w - render_w) / 2;
        render_y = area_y;
    }
    
    SDL_FRect destRect = {(float)render_x, (float)render_y, (float)render_w, (float)render_h};
    SDL_RenderTexture(bapi_internal_renderer, video->texture, NULL, &destRect);
}

void bapi_video_render_center(bapi_video_t video, int window_w, int window_h) {
    if (video == NULL || video->texture == NULL || bapi_internal_renderer == NULL) {
        return;
    }
    
    float video_aspect = (float)video->width / (float)video->height;
    float window_aspect = (float)window_w / (float)window_h;
    
    int render_w, render_h, render_x, render_y;
    
    if (video_aspect > window_aspect) {
        render_w = window_w;
        render_h = (int)(window_w / video_aspect);
        render_x = 0;
        render_y = (window_h - render_h) / 2;
    } else {
        render_h = window_h;
        render_w = (int)(window_h * video_aspect);
        render_x = (window_w - render_w) / 2;
        render_y = 0;
    }
    
    SDL_FRect destRect = {(float)render_x, (float)render_y, (float)render_w, (float)render_h};
    SDL_RenderTexture(bapi_internal_renderer, video->texture, NULL, &destRect);
}

void bapi_video_set_loop(bapi_video_t video, int loop) {
    if (video != NULL) {
        video->loop = loop;
    }
}

void bapi_video_set_volume(bapi_video_t video, float volume) {
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

int bapi_video_is_playing(bapi_video_t video) {
    if (video == NULL) {
        return 0;
    }
    return video->playing && !video->paused;
}

void bapi_video_get_size(bapi_video_t video, int* w, int* h) {
    if (video == NULL) {
        if (w != NULL) *w = 0;
        if (h != NULL) *h = 0;
        return;
    }
    
    if (w != NULL) *w = video->width;
    if (h != NULL) *h = video->height;
}

void bapi_video_update(void) {
    if (g_current_video == NULL || !g_current_video->playing || g_current_video->paused) {
        return;
    }
    
    bapi_video_t video = g_current_video;
    
    Uint32 now = SDL_GetTicks();
    Uint32 frame_delay = (Uint32)(1000.0 / video->fps);
    
    if (now - video->last_update < frame_delay) {
        return;
    }
    
    video->last_update = now;
    
    if (decode_video_frame(video) < 0) {
        if (video->loop) {
            av_seek_frame(video->format_ctx, video->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(video->codec_ctx);
            video->current_time = 0;
            decode_video_frame(video);
        } else {
            video->playing = 0;
            g_current_video = NULL;
            printf("[VIDEO] Playback finished: %s\n", video->filepath);
        }
    }
}
