#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct plat_window* plat_window_t;
typedef struct plat_renderer* plat_renderer_t;
typedef struct plat_texture* plat_texture_t;
typedef struct plat_font* plat_font_t;
typedef struct plat_audio_device* plat_audio_device_t;
typedef struct plat_audio_stream* plat_audio_stream_t;
typedef struct plat_mutex* plat_mutex_t;
typedef struct plat_surface plat_surface_t;
typedef struct plat_io plat_io_t;

typedef struct {
	float x;
	float y;
} plat_pointf_t;

typedef struct {
	float x;
	float y;
	float w;
	float h;
} plat_rectf_t;

typedef struct {
	float	 x;
	float	 y;
	float	 u;
	float	 v;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} plat_vertex_t;

#define PLAT_FLIP_HORIZONTAL 1
#define PLAT_FLIP_VERTICAL	 2



typedef enum {
	PLAT_EVENT_QUIT,
	PLAT_EVENT_KEY_DOWN,
	PLAT_EVENT_KEY_UP,
	PLAT_EVENT_MOUSE_BUTTON_DOWN,
	PLAT_EVENT_MOUSE_BUTTON_UP,
	PLAT_EVENT_MOUSE_MOTION,
	PLAT_EVENT_MOUSE_WHEEL,
	PLAT_EVENT_WINDOW_RESIZED,
} plat_event_type_t;


#define PLAT_BUTTON_LEFT  1
#define PLAT_BUTTON_RIGHT 3


typedef struct {
	plat_event_type_t type;
	union {
		struct {
			uint32_t key;
		} key;
		struct {
			float x;
			float y;
			int button;
		} button;
		struct {
			float x;
			float y;
		} motion;
		struct {
			float x;
			float y;
			float mouse_x;
			float mouse_y;
		} wheel;
		struct {
			int width;
			int height;
		} window;
	} data;
} plat_event_t;



enum plat_key_code {
	PLAT_KEY_ESC = 128,
	PLAT_KEY_BACKSPACE,
	PLAT_KEY_TAB,
	PLAT_KEY_ENTER,
	PLAT_KEY_CAPS,
	PLAT_KEY_SHIFT,
	PLAT_KEY_CTRL,
	PLAT_KEY_ALT,
	PLAT_KEY_SPACE,
	PLAT_KEY_F1,
	PLAT_KEY_F2,
	PLAT_KEY_F3,
	PLAT_KEY_F4,
	PLAT_KEY_F5,
	PLAT_KEY_F6,
	PLAT_KEY_F7,
	PLAT_KEY_F8,
	PLAT_KEY_F9,
	PLAT_KEY_F10,
	PLAT_KEY_F11,
	PLAT_KEY_F12,
	PLAT_KEY_NUML,
	PLAT_KEY_SCROLL
};



typedef enum {
	PLAT_AUDIO_F32,
} plat_audio_format_t;

typedef struct {
	plat_audio_format_t format;
	int channels;
	int freq;
} plat_audio_spec_t;



typedef enum {
	PLAT_PIXELFORMAT_ARGB8888,
	PLAT_PIXELFORMAT_BC1,
	PLAT_PIXELFORMAT_BC3,
} plat_pixel_format_t;

typedef enum {
	PLAT_TEXTUREACCESS_STREAMING,
} plat_texture_access_t;

typedef enum {
	PLAT_BLENDMODE_BLEND,
} plat_blend_mode_t;



#define PLAT_INIT_VIDEO 0x00000001
#define PLAT_INIT_AUDIO 0x00000010

#ifdef __cplusplus
}
#endif
