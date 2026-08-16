#pragma once

#include "platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int (*init)(uint32_t flags);
	void (*quit)(void);
	void (*delay)(uint32_t ms);
	uint32_t (*get_ticks)(void);
	void (*log_debug)(const char *fmt, ...);
	void (*log_info)(const char *fmt, ...);
	void (*log_warn)(const char *fmt, ...);
	void (*log_error)(const char *fmt, ...);
	void (*log_critical)(const char *fmt, ...);
} plat_core_api_t;

typedef struct {
	plat_window_t (*create_window)(const char *title, int width, int height);
	void (*destroy_window)(plat_window_t window);
	int (*poll_event)(plat_event_t *event);
	void (*get_mouse_state)(float *x, float *y);
} plat_window_api_t;

typedef struct {
	plat_renderer_t (*create_renderer)(plat_window_t window);
	void (*destroy_renderer)(plat_renderer_t renderer);
	void (*set_render_draw_color)(plat_renderer_t renderer, uint8_t r, uint8_t g, uint8_t b,
								  uint8_t a);
	void (*set_render_draw_blend_mode)(plat_renderer_t renderer, plat_blend_mode_t mode);
	void (*render_clear)(plat_renderer_t renderer);
	void (*render_present)(plat_renderer_t renderer);
	void (*render_point)(plat_renderer_t renderer, float x, float y);
	void (*render_line)(plat_renderer_t renderer, float x1, float y1, float x2, float y2);
	void (*render_rect)(plat_renderer_t renderer, float x, float y, float w, float h);
	void (*render_fill_rect)(plat_renderer_t renderer, float x, float y, float w, float h);
	void (*render_texture)(plat_renderer_t renderer, plat_texture_t texture, float x, float y,
						   float w, float h);
} plat_renderer_api_t;

typedef struct {
	plat_texture_t (*create_texture_from_surface)(plat_renderer_t renderer,
												  plat_surface_t *surface);
	plat_texture_t (*create_texture)(plat_renderer_t renderer, plat_pixel_format_t format,
									 plat_texture_access_t access, int width, int height);
	int (*update_texture)(plat_texture_t texture, const void *pixels, int pitch);
	int (*get_texture_size)(plat_texture_t texture, int *width, int *height);
	void (*destroy_texture)(plat_texture_t texture);

	plat_surface_t *(*load_image)(const char *filepath);
	void (*destroy_surface)(plat_surface_t *surface);
} plat_texture_api_t;

typedef struct {
	int (*init_ttf)(void);
	void (*quit_ttf)(void);
	plat_font_t (*open_font)(const char *filepath, float size);
	void (*close_font)(plat_font_t font);
	plat_surface_t *(*render_text_blended)(plat_font_t font, const char *text, int len, uint8_t r,
										   uint8_t g, uint8_t b, uint8_t a);
	int (*get_string_size)(plat_font_t font, const char *text, int len, int *w, int *h);
} plat_text_api_t;

typedef struct {
	plat_audio_device_t (*open_audio_device)(plat_audio_format_t format, int channels, int freq);
	void (*close_audio_device)(plat_audio_device_t device);
	plat_audio_stream_t (*create_audio_stream)(const plat_audio_spec_t *spec);
	plat_audio_stream_t (*open_audio_device_stream)(plat_audio_format_t format, int channels,
													int freq);
	int (*bind_audio_stream)(plat_audio_device_t device, plat_audio_stream_t stream);
	void (*destroy_audio_stream)(plat_audio_stream_t stream);
	int (*put_audio_stream_data)(plat_audio_stream_t stream, const void *data, int len);
	int (*flush_audio_stream)(plat_audio_stream_t stream);
	int (*get_audio_stream_queued)(plat_audio_stream_t stream);
	void (*clear_audio_stream)(plat_audio_stream_t stream);
	int (*set_audio_stream_gain)(plat_audio_stream_t stream, float gain);
	void (*resume_audio_stream_device)(plat_audio_stream_t stream);
	int (*load_wav)(const char *filepath, plat_audio_spec_t *spec, uint8_t **buffer,
					uint32_t *length);
	void (*mem_free)(void *ptr);
} plat_audio_api_t;

typedef struct {
	plat_mutex_t (*create_mutex)(void);
	void (*destroy_mutex)(plat_mutex_t mutex);
	void (*lock_mutex)(plat_mutex_t mutex);
	void (*unlock_mutex)(plat_mutex_t mutex);
} plat_sync_api_t;

typedef struct {
	plat_io_t *(*open_read)(const char *path);
	void	   (*close)(plat_io_t *io);
	size_t	   (*read)(plat_io_t *io, void *buf, size_t size);
	int64_t	   (*seek)(plat_io_t *io, int64_t offset, int whence);
	int64_t	   (*tell)(plat_io_t *io);
	int64_t	   (*size)(plat_io_t *io);
} plat_io_api_t;

typedef struct {
	plat_core_api_t		core;
	plat_window_api_t	window;
	plat_renderer_api_t renderer;
	plat_texture_api_t	texture;
	plat_text_api_t		text;
	plat_audio_api_t	audio;
	plat_sync_api_t		sync;
	plat_io_api_t		io;
	uint32_t			capabilities;
} plat_interface_t;

#define PLAT_CAPABILITY_AUDIO 0x00000001u
#define PLAT_CAPABILITY_VIDEO 0x00000002u

int plat_init(const plat_interface_t *interface);

const plat_interface_t *plat_get(void);

static inline int plat_supports(uint32_t capability)
{
	const plat_interface_t *platform = plat_get();
	return platform != 0 && (platform->capabilities & capability) != 0;
}

void plat_shutdown(void);

const plat_interface_t *plat_sdl3_interface(void);

const plat_interface_t *plat_xj380_interface(void);

const plat_interface_t *plat_minios_interface(void);

#ifdef __cplusplus
}
#endif
