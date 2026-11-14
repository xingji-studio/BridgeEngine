#pragma once

#include "bridgeengine_version.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_window_internal		   *bapi_window_t;
typedef struct bapi_renderer_internal	   *bapi_renderer_t;
typedef struct bapi_texture_internal	   *bapi_texture_t;
typedef struct bapi_sound_internal		   *bapi_sound_t;
typedef struct bapi_video_internal		   *bapi_video_t;
typedef struct bapi_scene_internal		   *bapi_scene_t;
typedef struct bapi_scene_manager_internal *bapi_scene_manager_t;
typedef struct bapi_level_internal		   *bapi_level_t;
typedef struct bapi_level_manager_internal *bapi_level_manager_t;
typedef struct bapi_ui_internal			   *bapi_ui_t;
typedef struct bapi_ui_component_internal  *bapi_ui_component_t;
typedef struct bapi_project_internal	   *bapi_project_t;
typedef struct bapi_file_internal		   *bapi_file_t;
typedef struct bapi_pack_internal		   *bapi_pack_t;
typedef struct bapi_pack_stream_internal   *bapi_pack_stream_t;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} bapi_color_t;

typedef struct {
	float x;
	float y;
	float w;
	float h;
} bapi_rect_t;

typedef enum {
	BAPI_EVENT_QUIT				 = 0,
	BAPI_EVENT_KEY_DOWN			 = 1,
	BAPI_EVENT_MOUSE_BUTTON_DOWN = 2,
	BAPI_EVENT_MOUSE_BUTTON_UP	 = 3,
	BAPI_EVENT_MOUSE_MOTION		 = 4,
	BAPI_EVENT_KEY_UP			 = 5,
	BAPI_EVENT_MOUSE_WHEEL		 = 6,
	BAPI_EVENT_UNKNOWN			 = 7,
	BAPI_EVENT_TEXT_INPUT		 = 8,
	BAPI_EVENT_WINDOW_RESIZED	 = 9,
} bapi_event_type_t;

typedef struct {
	bapi_event_type_t type;
	union {
		struct {
			uint32_t key;
		} key;
		struct {
			float x;
			float y;
			int	  button;
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
			char text[32];
		} text;
		struct {
			int width;
			int height;
		} window;
	} data;
} bapi_event_t;

#define BAPI_BUTTON_LEFT		 1
#define BAPI_MAX_CACHED_TEXTURES 64

typedef struct {
	bapi_rect_t	 rect;
	bapi_color_t normal_color;
	bapi_color_t hover_color;
	bapi_color_t click_color;
	const char	*text;
	bapi_color_t text_color;
	float		 text_size;
	float		 text_width;
	float		 text_height;
	int			 is_clicked;
	int			 is_hovered;
} bapi_button_t;

enum special_key_code {
	KEY_ESC = 128,
	KEY_BACKSPACE,
	KEY_TAB,
	KEY_ENTER,
	KEY_CAPS,
	KEY_SHIFT,
	KEY_CTRL,
	KEY_ALT,
	KEY_SPACE,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_NUML,
	KEY_SCROLL,
};

typedef struct {
	float x;
	float y;
} bapi_vec2_t;

typedef struct {
	float x;
	float y;
	float r;
} bapi_circle_t;

typedef struct {
	float x;
	float y;
	float zoom;
	float viewport_w;
	float viewport_h;
	float rotation;
} bapi_camera_t;

typedef void (*bapi_scene_on_enter_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_exit_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_update_fn)(bapi_scene_t scene, float delta_time);
typedef void (*bapi_scene_on_render_fn)(bapi_scene_t scene);

typedef struct {
	bapi_scene_on_enter_fn	on_enter;
	bapi_scene_on_exit_fn	on_exit;
	bapi_scene_on_update_fn on_update;
	bapi_scene_on_render_fn on_render;
	void				   *user_data;
} bapi_scene_callbacks_t;

typedef void (*bapi_level_on_load_fn)(bapi_level_t level);
typedef void (*bapi_level_on_unload_fn)(bapi_level_t level);
typedef void (*bapi_level_on_update_fn)(bapi_level_t level, float delta_time);
typedef void (*bapi_level_on_render_fn)(bapi_level_t level);

typedef struct {
	bapi_level_on_load_fn	on_load;
	bapi_level_on_unload_fn on_unload;
	bapi_level_on_update_fn on_update;
	bapi_level_on_render_fn on_render;
	void				   *user_data;
} bapi_level_callbacks_t;

typedef enum {
	BAPI_LOG_LEVEL_DEBUG = 0,
	BAPI_LOG_LEVEL_INFO,
	BAPI_LOG_LEVEL_WARN,
	BAPI_LOG_LEVEL_ERROR,
	BAPI_LOG_LEVEL_CRITICAL,
	BAPI_LOG_LEVEL_NONE,
} bapi_log_level_t;

typedef enum {
	BAPI_UI_COMPONENT_RECT = 0,
	BAPI_UI_COMPONENT_LABEL,
	BAPI_UI_COMPONENT_BUTTON,
	BAPI_UI_COMPONENT_IMAGE,
	BAPI_UI_COMPONENT_LINE,
	BAPI_UI_COMPONENT_CIRCLE,
	BAPI_UI_COMPONENT_POLYGON,
	BAPI_UI_COMPONENT_BORDER,
	BAPI_UI_COMPONENT_PROGRESS,
	BAPI_UI_COMPONENT_SEPARATOR,
	BAPI_UI_COMPONENT_PANEL,
	BAPI_UI_COMPONENT_CONTAINER,
	BAPI_UI_COMPONENT_ROW,
	BAPI_UI_COMPONENT_COLUMN,
	BAPI_UI_COMPONENT_GRID,
	BAPI_UI_COMPONENT_CHECKBOX,
	BAPI_UI_COMPONENT_RADIO,
	BAPI_UI_COMPONENT_TOGGLE,
	BAPI_UI_COMPONENT_SLIDER,
	BAPI_UI_COMPONENT_INPUT,
	BAPI_UI_COMPONENT_SELECT,
	BAPI_UI_COMPONENT_LIST,
	BAPI_UI_COMPONENT_SCROLL,
	BAPI_UI_COMPONENT_TAB,
	BAPI_UI_COMPONENT_VIDEO,
	BAPI_UI_COMPONENT_CANVAS,
	BAPI_UI_COMPONENT_NINE_PATCH,
	BAPI_UI_COMPONENT_ANIMATION,
	BAPI_UI_COMPONENT_TOOLTIP,
	BAPI_UI_COMPONENT_MODAL,
	BAPI_UI_COMPONENT_POPUP,
} bapi_ui_component_type_t;

typedef enum {
	BAPI_UI_COLOR_NORMAL = 0,
	BAPI_UI_COLOR_HOVER,
	BAPI_UI_COLOR_CLICK,
	BAPI_UI_COLOR_TEXT,
} bapi_ui_color_role_t;

typedef struct {
	bapi_log_level_t min_level;
	bool			 use_colors;
	bool			 use_file;
	const char		*log_file_path;
} bapi_log_config_t;

typedef enum {
	BAPI_RENDERER_AUTO	 = 0,
	BAPI_RENDERER_SDL	 = 1,
	BAPI_RENDERER_VULKAN = 2,
} bapi_renderer_backend_t;

int bapi_engine_init(const char *title, int width, int height);
int bapi_engine_init_ex(const char *title, int width, int height, bapi_renderer_backend_t backend);
bapi_renderer_backend_t bapi_engine_get_renderer_backend(void);
/* Releases all engine resources; resource handles are invalid after this call. */
void bapi_engine_quit(void);
/* Borrowed handles; valid only from bapi_engine_init() through bapi_engine_quit(). */
bapi_window_t	bapi_engine_get_window(void);
bapi_renderer_t bapi_engine_get_renderer(void);

int		bapi_poll_event(bapi_event_t *event);
int		bapi_event_get_type(const bapi_event_t *event);
uint8_t bapi_event_get_key_code(const bapi_event_t *event);
int		bapi_event_get_mouse_x(const bapi_event_t *event);
int		bapi_event_get_mouse_y(const bapi_event_t *event);
int		bapi_event_get_mouse_button(const bapi_event_t *event);
int		bapi_event_get_motion_x(const bapi_event_t *event);
int		bapi_event_get_motion_y(const bapi_event_t *event);
float	bapi_event_get_wheel_x(const bapi_event_t *event);
float	bapi_event_get_wheel_y(const bapi_event_t *event);
float	bapi_event_get_wheel_mouse_x(const bapi_event_t *event);
float	bapi_event_get_wheel_mouse_y(const bapi_event_t *event);
int		bapi_event_is_mouse_button_down(const bapi_event_t *event);
int		bapi_event_is_mouse_button_up(const bapi_event_t *event);
int		bapi_event_is_mouse_motion(const bapi_event_t *event);
int		bapi_event_get_window_width(const bapi_event_t *event);
int		bapi_event_get_window_height(const bapi_event_t *event);

void bapi_render_clear(void);
void bapi_render_present(void);
void bapi_set_render_color(bapi_color_t color);
void bapi_delay(uint32_t ms);
void bapi_draw_pixel(float x, float y, bapi_color_t color);
void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
						bapi_color_t color);
void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
void bapi_fill_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
						bapi_color_t color);
void bapi_draw_image(const char *filepath, float x, float y, float w, float h);
void bapi_draw_text(const char *text, float x, float y, float size, bapi_color_t color);
void bapi_get_text_size(const char *text, float size, float *width, float *height);
/* Legacy compatibility wrappers; `color` is 0xAARRGGBB packed as in bapi_color_from_hex(). */
void bapi_engine_render_drawpixel(int x, int y, int color);
void bapi_engine_render_fillrect(int x, int y, int width, int height, int color);
void bapi_engine_render_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int color);
void bapi_text_init(void);
void bapi_text_cleanup(void);

void bapi_mouse_init(void);
void bapi_mouse_handle_event(const bapi_event_t *event);
void bapi_mouse_render(void);
void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_mouse_clear(void);
void bapi_mouse_cleanup(void);

bapi_color_t bapi_color_from_hex(uint32_t hex_color);
bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t	 bapi_get_ticks(void);

/* --- Frame timing ---------------------------------------------------------
 * Optional game-loop helper. bapi_frame_begin() updates the delta and elapsed
 * clocks; bapi_frame_end() presents and applies pacing. Manual loops may keep
 * calling bapi_render_present() directly.
 */
float	 bapi_frame_begin(void);
void	 bapi_frame_end(void);
float	 bapi_get_delta_time(void);
float	 bapi_get_fps(void);
double	 bapi_get_elapsed_time(void);
uint64_t bapi_get_frame_count(void);
void	 bapi_set_target_fps(int fps);
void	 bapi_set_time_scale(float scale);
void	 bapi_set_frame_paused(int paused);
int		 bapi_frame_is_paused(void);
int		 bapi_frame_steps(float fixed_delta, int max_steps);
float	 bapi_frame_step_alpha(void);

/* --- Window / display ----------------------------------------------------- */
void bapi_set_vsync(int enabled);
int	 bapi_get_vsync(void);
void bapi_set_fullscreen(int enabled);
int	 bapi_get_fullscreen(void);
void bapi_set_logical_size(int width, int height);
void bapi_get_logical_size(int *width, int *height);
void bapi_get_window_size(int *width, int *height);

int			 bapi_audio_init(void);
void		 bapi_audio_cleanup(void);
void		 bapi_audio_set_master_volume(float volume);
float		 bapi_audio_get_master_volume(void);
void		 bapi_audio_stop_all(void);
int			 bapi_audio_get_active_sound_count(void);
bapi_sound_t bapi_sound_load(const char *filepath);
/* Decode WAV bytes held in memory. The input buffer is only read during the
 * call and is not kept; the decoded PCM is owned by the returned sound. */
bapi_sound_t bapi_sound_load_from_memory(const void *data, size_t size);
int			 bapi_sound_play(bapi_sound_t sound);
void		 bapi_sound_set_volume(bapi_sound_t sound, float volume);
void		 bapi_sound_set_loop(bapi_sound_t sound, int loop);
void		 bapi_sound_stop(bapi_sound_t sound);
void		 bapi_sound_update(void);
void		 bapi_sound_free(bapi_sound_t sound);

/* --- Streaming music ------------------------------------------------------
 * Compressed music (MP3/OGG/FLAC/WAV/etc, whatever FFmpeg decodes) is decoded
 * incrementally by bapi_music_update(), which must be called every frame while
 * music is playing. Requires a successful bapi_audio_init().
 */
typedef struct bapi_music_internal *bapi_music_t;

bapi_music_t bapi_music_load(const char *filepath);
bapi_music_t bapi_music_load_from_memory(const void *data, size_t size);
void		 bapi_music_free(bapi_music_t music);
int			 bapi_music_play(bapi_music_t music);
void		 bapi_music_pause(bapi_music_t music);
void		 bapi_music_resume(bapi_music_t music);
void		 bapi_music_stop(bapi_music_t music);
void		 bapi_music_set_volume(bapi_music_t music, float volume);
void		 bapi_music_set_loop(bapi_music_t music, int loop);
int			 bapi_music_is_playing(bapi_music_t music);
double		 bapi_music_get_duration(bapi_music_t music);
void		 bapi_music_update(void);

bapi_button_t *bapi_create_button(float x, float y, float w, float h, const char *text,
								  bapi_color_t normal_color, bapi_color_t hover_color,
								  bapi_color_t click_color, bapi_color_t text_color,
								  float text_size);
void		   bapi_destroy_button(bapi_button_t *button);
int			   bapi_button_update(bapi_button_t *button, const bapi_event_t *event);
void		   bapi_button_render(bapi_button_t *button);
int			   bapi_button_is_clicked(bapi_button_t *button);
int			   bapi_button_is_hovered(bapi_button_t *button);

void bapi_camera_init(bapi_camera_t *cam, float viewport_w, float viewport_h);
void bapi_camera_set_position(bapi_camera_t *cam, float x, float y);
void bapi_camera_move(bapi_camera_t *cam, float dx, float dy);
void bapi_camera_set_zoom(bapi_camera_t *cam, float zoom);
void bapi_camera_set_rotation(bapi_camera_t *cam, float angle_rad);
void bapi_camera_set_viewport(bapi_camera_t *cam, float w, float h);
void bapi_camera_world_to_screen(bapi_camera_t *cam, float wx, float wy, float *sx, float *sy);
void bapi_camera_screen_to_world(bapi_camera_t *cam, float sx, float sy, float *wx, float *wy);
bapi_vec2_t bapi_camera_world_to_screen_v(bapi_camera_t *cam, bapi_vec2_t world);
bapi_vec2_t bapi_camera_screen_to_world_v(bapi_camera_t *cam, bapi_vec2_t screen);
void		bapi_camera_get_view_rect(bapi_camera_t *cam, bapi_rect_t *out_rect);

/* --- 2D transform stack ---------------------------------------------------
 * The stack starts as identity. bapi_push_transform() applies scale, then
 * rotation, then translation, and concatenates the result with the current
 * transform. Every draw/texture call applies the top transform. Text is
 * positioned and scaled but not rotated.
 */
void		bapi_reset_transform(void);
void		bapi_push_transform(bapi_vec2_t translation, float rotation, bapi_vec2_t scale);
void		bapi_pop_transform(void);
void		bapi_apply_camera(const bapi_camera_t *camera);
bapi_vec2_t bapi_transform_point(bapi_vec2_t point);
bapi_vec2_t bapi_transform_vector(bapi_vec2_t vector);
float		bapi_transform_scale_factor(void);

void  bapi_input_init(void);
void  bapi_input_cleanup(void);
void  bapi_input_update(void);
void  bapi_input_handle_event(const bapi_event_t *event);
int	  bapi_is_key_down(uint8_t key);
int	  bapi_is_key_pressed(uint8_t key);
int	  bapi_is_key_released(uint8_t key);
int	  bapi_is_mouse_button_down(int button);
int	  bapi_is_mouse_button_pressed(int button);
int	  bapi_is_mouse_button_released(int button);
float bapi_get_mouse_x(void);
float bapi_get_mouse_y(void);
void  bapi_get_mouse_position(float *x, float *y);

bapi_level_t bapi_level_create(const char *name, int index, bapi_level_callbacks_t callbacks);
void		 bapi_level_destroy(bapi_level_t level);
const char	*bapi_level_get_name(bapi_level_t level);
int			 bapi_level_get_index(bapi_level_t level);
void		*bapi_level_get_user_data(bapi_level_t level);
void		 bapi_level_set_user_data(bapi_level_t level, void *user_data);
bapi_level_manager_t bapi_level_manager_create(void);
void				 bapi_level_manager_destroy(bapi_level_manager_t manager);
int					 bapi_level_manager_add_level(bapi_level_manager_t manager, bapi_level_t level);
int					 bapi_level_manager_load_level(bapi_level_manager_t manager, const char *name);
int			 bapi_level_manager_load_level_by_index(bapi_level_manager_t manager, int index);
int			 bapi_level_manager_next_level(bapi_level_manager_t manager);
int			 bapi_level_manager_previous_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_current_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_level(bapi_level_manager_t manager, const char *name);
bapi_level_t bapi_level_manager_get_level_by_index(bapi_level_manager_t manager, int index);
int			 bapi_level_manager_get_level_count(bapi_level_manager_t manager);
void		 bapi_level_manager_update(bapi_level_manager_t manager, float delta_time);
void		 bapi_level_manager_render(bapi_level_manager_t manager);

/* Last error message set by the engine (empty string when none). */
const char *bapi_get_last_error(void);
void		bapi_clear_last_error(void);

bool bapi_log_init(const bapi_log_config_t *config);
void bapi_log_shutdown(void);
void bapi_log_set_level(bapi_log_level_t level);
void bapi_log_message(bapi_log_level_t level, const char *file, int line, const char *func,
					  const char *format, ...);

bapi_vec2_t	  bapi_vec2(float x, float y);
bapi_vec2_t	  bapi_vec2_add(bapi_vec2_t a, bapi_vec2_t b);
bapi_vec2_t	  bapi_vec2_sub(bapi_vec2_t a, bapi_vec2_t b);
bapi_vec2_t	  bapi_vec2_scale(bapi_vec2_t vector, float scalar);
bapi_vec2_t	  bapi_vec2_negate(bapi_vec2_t vector);
float		  bapi_vec2_dot(bapi_vec2_t a, bapi_vec2_t b);
float		  bapi_vec2_length(bapi_vec2_t vector);
float		  bapi_vec2_length_sq(bapi_vec2_t vector);
bapi_vec2_t	  bapi_vec2_normalize(bapi_vec2_t vector);
float		  bapi_vec2_distance(bapi_vec2_t a, bapi_vec2_t b);
bapi_vec2_t	  bapi_vec2_lerp(bapi_vec2_t a, bapi_vec2_t b, float t);
bapi_vec2_t	  bapi_vec2_rotate(bapi_vec2_t vector, float angle);
int			  bapi_vec2_equals(bapi_vec2_t a, bapi_vec2_t b);
bapi_circle_t bapi_circle(float x, float y, float radius);
/*
 * Geometry queries require finite coordinates, non-negative circle radii, and
 * non-negative rectangle widths and heights. Results outside these preconditions
 * are unspecified. Rectangle/AABB queries require positive-area overlap, while
 * circle queries count touching as an intersection.
 */
int			bapi_rect_contains_point(bapi_rect_t rect, bapi_vec2_t point);
int			bapi_rect_intersects(bapi_rect_t a, bapi_rect_t b);
bapi_rect_t bapi_rect_intersection(bapi_rect_t a, bapi_rect_t b);
bapi_rect_t bapi_rect_union(bapi_rect_t a, bapi_rect_t b);
bapi_vec2_t bapi_rect_center(bapi_rect_t rect);
int			bapi_circle_contains_point(bapi_circle_t circle, bapi_vec2_t point);
int			bapi_circle_intersects_circle(bapi_circle_t a, bapi_circle_t b);
int			bapi_circle_intersects_rect(bapi_circle_t circle, bapi_rect_t rect);
/* Alias of bapi_rect_intersects with identical positive-area semantics. */
int	  bapi_collision_aabb(bapi_rect_t a, bapi_rect_t b);
float bapi_clamp(float value, float min, float max);
float bapi_lerp(float a, float b, float t);
float bapi_deg_to_rad(float degrees);
float bapi_rad_to_deg(float radians);

bapi_scene_t		 bapi_scene_create(const char *name, bapi_scene_callbacks_t callbacks);
void				 bapi_scene_destroy(bapi_scene_t scene);
const char			*bapi_scene_get_name(bapi_scene_t scene);
void				*bapi_scene_get_user_data(bapi_scene_t scene);
void				 bapi_scene_set_user_data(bapi_scene_t scene, void *user_data);
bapi_scene_manager_t bapi_scene_manager_create(void);
void				 bapi_scene_manager_destroy(bapi_scene_manager_t manager);
int					 bapi_scene_manager_add_scene(bapi_scene_manager_t manager, bapi_scene_t scene);
int			 bapi_scene_manager_switch_scene(bapi_scene_manager_t manager, const char *name);
bapi_scene_t bapi_scene_manager_get_current_scene(bapi_scene_manager_t manager);
bapi_scene_t bapi_scene_manager_get_scene(bapi_scene_manager_t manager, const char *name);
void		 bapi_scene_manager_update(bapi_scene_manager_t manager, float delta_time);
void		 bapi_scene_manager_render(bapi_scene_manager_t manager);

bapi_texture_t bapi_texture_load(const char *filepath);
void		   bapi_texture_destroy(bapi_texture_t texture);
void		   bapi_texture_render(bapi_texture_t texture, float x, float y);
void		   bapi_texture_render_ex(bapi_texture_t texture, float x, float y, float w, float h);
void		   bapi_texture_get_size(bapi_texture_t texture, int *w, int *h);
void		   bapi_texture_cache_clear(void);
bapi_texture_t bapi_texture_from_file(const char *filepath, int *out_w, int *out_h);

#define BAPI_FLIP_HORIZONTAL 1
#define BAPI_FLIP_VERTICAL	 2

/* Sprite draw parameters. `src` with zero width/height means the whole
 * texture; `pivot_x`/`pivot_y` are normalized (0..1) inside `dst` and default
 * to 0.5/0.5 for centered rotation; `tint` is multiplied into the texture
 * (255/255/255/255 leaves it unchanged). */
typedef struct {
	bapi_rect_t	 src;
	bapi_rect_t	 dst;
	float		 pivot_x;
	float		 pivot_y;
	float		 rotation;
	int			 flip;
	bapi_color_t tint;
} bapi_texture_draw_t;

bapi_texture_draw_t bapi_texture_draw_default(bapi_rect_t dst);
void				bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params);

/* --- Scene entities -------------------------------------------------------
 * Lightweight transform hierarchy with optional sprite drawing. Children are
 * owned by their parent: destroying a parent destroys its whole subtree.
 * Transform order is scale, then rotation, then translation. An entity with a
 * sprite is positioned at the sprite center (pivot 0.5, 0.5).
 */
typedef struct bapi_entity_internal *bapi_entity_t;
typedef void (*bapi_entity_update_fn)(bapi_entity_t entity, float delta_time);

bapi_entity_t bapi_entity_create(const char *name);
void		  bapi_entity_destroy(bapi_entity_t entity);
const char	 *bapi_entity_get_name(bapi_entity_t entity);
void		 *bapi_entity_get_user_data(bapi_entity_t entity);
void		  bapi_entity_set_user_data(bapi_entity_t entity, void *user_data);

void		bapi_entity_set_position(bapi_entity_t entity, bapi_vec2_t position);
bapi_vec2_t bapi_entity_get_position(bapi_entity_t entity);
void		bapi_entity_set_rotation(bapi_entity_t entity, float radians);
float		bapi_entity_get_rotation(bapi_entity_t entity);
void		bapi_entity_set_scale(bapi_entity_t entity, bapi_vec2_t scale);
bapi_vec2_t bapi_entity_get_scale(bapi_entity_t entity);
bapi_vec2_t bapi_entity_get_world_position(bapi_entity_t entity);
float		bapi_entity_get_world_rotation(bapi_entity_t entity);
bapi_vec2_t bapi_entity_get_world_scale(bapi_entity_t entity);

int			  bapi_entity_add_child(bapi_entity_t parent, bapi_entity_t child);
int			  bapi_entity_remove_child(bapi_entity_t parent, bapi_entity_t child);
bapi_entity_t bapi_entity_get_parent(bapi_entity_t entity);
int			  bapi_entity_get_child_count(bapi_entity_t entity);
bapi_entity_t bapi_entity_get_child(bapi_entity_t entity, int index);
bapi_entity_t bapi_entity_find_child(bapi_entity_t entity, const char *name);

void bapi_entity_set_visible(bapi_entity_t entity, int visible);
int	 bapi_entity_is_visible(bapi_entity_t entity);
void bapi_entity_set_texture(bapi_entity_t entity, bapi_texture_t texture, bapi_rect_t source);
void bapi_entity_set_tint(bapi_entity_t entity, bapi_color_t tint);

bapi_texture_t bapi_entity_get_texture(bapi_entity_t entity);
bapi_rect_t	   bapi_entity_get_source(bapi_entity_t entity);
bapi_color_t   bapi_entity_get_tint(bapi_entity_t entity);
void bapi_entity_set_update(bapi_entity_t entity, bapi_entity_update_fn update);
void bapi_entity_update(bapi_entity_t entity, float delta_time);
void bapi_entity_draw(bapi_entity_t entity);

/* --- World runtime -------------------------------------------------------
 * A world owns root entities, updates them in insertion order and draws the
 * whole tree under its active camera. Despawn is deferred to the end of the
 * current update so callbacks may safely destroy entities.
 */
typedef struct bapi_world_internal *bapi_world_t;

bapi_world_t  bapi_world_create(void);
void		  bapi_world_destroy(bapi_world_t world);
bapi_entity_t bapi_world_spawn(bapi_world_t world, const char *name);
void		  bapi_world_despawn(bapi_world_t world, bapi_entity_t entity);
void		  bapi_world_update(bapi_world_t world, float delta_time);
void		  bapi_world_draw(bapi_world_t world);
void		  bapi_world_set_camera(bapi_world_t world, bapi_camera_t camera);
void		  bapi_world_get_camera(bapi_world_t world, bapi_camera_t *out_camera);
int			  bapi_world_entity_count(bapi_world_t world);
bapi_entity_t bapi_world_get_entity(bapi_world_t world, int index);
void		  bapi_world_set_paused(bapi_world_t world, int paused);
int			  bapi_world_is_paused(bapi_world_t world);

/* --- Input actions -------------------------------------------------------
 * Name-based bindings on top of raw keyboard/mouse state. Actions can have
 * multiple bindings; down/pressed/released are true when any binding is true.
 */
int bapi_input_action_clear(void);
int bapi_input_action_bind(const char *name, uint8_t key);
int bapi_input_action_bind_mouse(const char *name, int button);
int bapi_input_action_down(const char *name);
int bapi_input_action_pressed(const char *name);
int bapi_input_action_released(const char *name);

/* --- Tilemap -------------------------------------------------------------
 * Row-major tile grid. tile_id -1 means empty. A tileset texture is optional:
 * when absent, non-empty tiles are drawn as solid colored squares.
 */
typedef struct bapi_tilemap_internal *bapi_tilemap_t;

bapi_tilemap_t bapi_tilemap_create(int width, int height, int tile_width, int tile_height);
void		   bapi_tilemap_destroy(bapi_tilemap_t tilemap);
int	 bapi_tilemap_set_tileset(bapi_tilemap_t tilemap, bapi_texture_t texture, int columns);
void bapi_tilemap_set_tile(bapi_tilemap_t tilemap, int x, int y, int tile_id, int solid);
int	 bapi_tilemap_get_tile(bapi_tilemap_t tilemap, int x, int y);
int	 bapi_tilemap_is_solid(bapi_tilemap_t tilemap, int x, int y);
int	 bapi_tilemap_collide_rect(bapi_tilemap_t tilemap, bapi_rect_t rect);
int	 bapi_tilemap_move_rect(bapi_tilemap_t tilemap, bapi_rect_t *rect, bapi_vec2_t delta);
void bapi_tilemap_draw(bapi_tilemap_t tilemap);

/* --- Sprite animation ----------------------------------------------------
 * Clips store a texture, frame rectangles and per-frame durations. Optional
 * bool parameters + transitions form a tiny state machine evaluated before
 * frame advancement.
 */
typedef struct bapi_animator_internal *bapi_animator_t;

bapi_animator_t bapi_animator_create(void);
void			bapi_animator_destroy(bapi_animator_t animator);
int	 bapi_animator_add_clip(bapi_animator_t animator, const char *name, bapi_texture_t texture,
							const bapi_rect_t *frames, const float *durations, int frame_count,
							int loop);
int	 bapi_animator_play(bapi_animator_t animator, const char *name);
int	 bapi_animator_add_transition(bapi_animator_t animator, const char *from, const char *to,
								  const char *parameter, int value);
void bapi_animator_set_bool(bapi_animator_t animator, const char *parameter, int value);
void bapi_animator_update(bapi_animator_t animator, float delta_time);
bapi_rect_t		bapi_animator_get_frame(bapi_animator_t animator);
bapi_texture_t	bapi_animator_get_texture(bapi_animator_t animator);
const char	   *bapi_animator_get_clip(bapi_animator_t animator);
int				bapi_animator_is_finished(bapi_animator_t animator);

/* --- Serialization -------------------------------------------------------
 * Text formats with an explicit version header. Parsers ignore unknown keys so
 * a newer file still loads on an older runtime; a missing or future version is
 * rejected. Saves are written atomically where the platform allows it.
 *
 *   .btm     tilemap grid, tileset reference, per-tile solid flags
 *   .bworld  entity tree with transform, visibility, sprite and tint
 *
 * Sprite and tileset references are texture paths. Loading re-opens them via
 * bapi_texture_load(), so a missing asset fails the load instead of rendering
 * an empty sprite.
 */
#define BAPI_SERIALIZE_VERSION 1

int			   bapi_tilemap_save(bapi_tilemap_t tilemap, const char *filepath);
bapi_tilemap_t bapi_tilemap_load(const char *filepath);
bapi_tilemap_t bapi_tilemap_load_from_memory(const char *text, size_t size);

int bapi_world_save(bapi_world_t world, const char *filepath);
int bapi_world_load(bapi_world_t world, const char *filepath);
int bapi_world_load_from_memory(bapi_world_t world, const char *text, size_t size);

/* Path a texture was loaded from, or NULL for memory/atlas-owned textures. */
const char *bapi_texture_get_path(bapi_texture_t texture);

/* --- Asset manifest ------------------------------------------------------
 * A manifest lists project asset paths relative to the project root. Verify
 * reports every path that is missing so a startup check can fail fast with a
 * concrete list instead of a black screen.
 */
#define BAPI_ASSET_MANIFEST_VERSION 1

typedef struct bapi_asset_manifest_internal *bapi_asset_manifest_t;

bapi_asset_manifest_t bapi_asset_manifest_load(const char *filepath);
void				  bapi_asset_manifest_destroy(bapi_asset_manifest_t manifest);
int					  bapi_asset_manifest_count(bapi_asset_manifest_t manifest);
const char			 *bapi_asset_manifest_get(bapi_asset_manifest_t manifest, int index);
/* Returns the number of missing entries; *checked receives the entry count. */
int bapi_asset_manifest_verify(bapi_asset_manifest_t manifest, const char *root,
							   int *checked);
/* Path of the first entry reported missing by the last verify, or NULL. */
const char *bapi_asset_manifest_first_missing(bapi_asset_manifest_t manifest);

/* --- Grid pathfinding ---------------------------------------------------
 * A* over a bapi_tilemap_t. Solid tiles are blocked. The returned path
 * excludes the start tile and includes the goal tile.
 */
typedef struct bapi_path_internal *bapi_path_t;

bapi_path_t bapi_path_find(bapi_tilemap_t map, int start_x, int start_y, int goal_x, int goal_y,
						   int allow_diagonal);
void		bapi_path_destroy(bapi_path_t path);
int			bapi_path_count(bapi_path_t path);
int			bapi_path_get(bapi_path_t path, int index, int *x, int *y);

/* --- Timers --------------------------------------------------------------
 * Frame-driven one-shot and repeating timers. Callbacks run from
 * bapi_timer_manager_update(); cancelling from inside a callback is safe.
 */
typedef struct bapi_timer_manager_internal *bapi_timer_manager_t;
typedef void (*bapi_timer_fn)(void *user_data);

bapi_timer_manager_t bapi_timer_manager_create(void);
void				 bapi_timer_manager_destroy(bapi_timer_manager_t manager);
int bapi_timer_after(bapi_timer_manager_t manager, float seconds, bapi_timer_fn callback,
					 void *user_data);
int bapi_timer_every(bapi_timer_manager_t manager, float interval, bapi_timer_fn callback,
					 void *user_data);
void bapi_timer_cancel(bapi_timer_manager_t manager, int timer_id);
void bapi_timer_manager_update(bapi_timer_manager_t manager, float delta_time);
int	 bapi_timer_count(bapi_timer_manager_t manager);

/* --- AABB collision world ------------------------------------------------
 * Axis-aligned colliders with layer/mask filtering. `move` resolves one
 * collider against solid colliders, X axis first and then Y.
 */
typedef struct bapi_collision_world_internal *bapi_collision_world_t;
typedef struct bapi_collider_internal	   *bapi_collider_t;

bapi_collision_world_t bapi_collision_world_create(void);
void				   bapi_collision_world_destroy(bapi_collision_world_t world);
bapi_collider_t bapi_collision_world_add(bapi_collision_world_t world, bapi_rect_t bounds,
										 uint32_t layer, uint32_t mask, int solid);
void			bapi_collision_world_remove(bapi_collision_world_t world, bapi_collider_t collider);
void			bapi_collider_set_bounds(bapi_collider_t collider, bapi_rect_t bounds);
bapi_rect_t		bapi_collider_get_bounds(bapi_collider_t collider);
void			bapi_collider_set_layer_mask(bapi_collider_t collider, uint32_t layer,
											uint32_t mask);
int bapi_collision_world_query(bapi_collision_world_t world, bapi_rect_t bounds, uint32_t layer,
							   bapi_collider_t *out, int max);
int bapi_collision_world_move(bapi_collision_world_t world, bapi_collider_t collider,
							  bapi_vec2_t delta, int *hit);

/* --- Camera controller ---------------------------------------------------
 * Smooth follow camera with deadzone, world bounds and a short deterministic
 * shake.
 */
typedef struct bapi_camera_controller_internal *bapi_camera_controller_t;

bapi_camera_controller_t bapi_camera_controller_create(float viewport_w, float viewport_h);
void					 bapi_camera_controller_destroy(bapi_camera_controller_t controller);
void bapi_camera_controller_set_bounds(bapi_camera_controller_t controller, bapi_rect_t bounds);
void bapi_camera_controller_set_deadzone(bapi_camera_controller_t controller, float width,
										 float height);
void bapi_camera_controller_set_smoothing(bapi_camera_controller_t controller, float smoothing);
void bapi_camera_controller_set_viewport(bapi_camera_controller_t controller, float width,
										 float height);
void bapi_camera_controller_follow(bapi_camera_controller_t controller, float x, float y);
void bapi_camera_controller_snap(bapi_camera_controller_t controller);
void bapi_camera_controller_update(bapi_camera_controller_t controller, float delta_time,
								   float target_x, float target_y);
void bapi_camera_controller_get_camera(bapi_camera_controller_t controller, bapi_camera_t *out);
void bapi_camera_controller_shake(bapi_camera_controller_t controller, float amplitude,
								  float duration);

/* --- Tween ---------------------------------------------------------------
 * Manager-owned float tweens with easing. A tween evaluates [from, to] over
 * its duration; loop restarts it automatically.
 */
typedef enum {
	BAPI_EASE_LINEAR = 0,
	BAPI_EASE_IN_QUAD,
	BAPI_EASE_OUT_QUAD,
	BAPI_EASE_IN_OUT_QUAD,
	BAPI_EASE_IN_CUBIC,
	BAPI_EASE_OUT_CUBIC,
	BAPI_EASE_IN_OUT_CUBIC,
	BAPI_EASE_OUT_BACK,
} bapi_ease_t;

typedef struct bapi_tween_internal		   *bapi_tween_t;
typedef struct bapi_tween_manager_internal *bapi_tween_manager_t;

float			  bapi_ease(bapi_ease_t ease, float t);
bapi_tween_manager_t bapi_tween_manager_create(void);
void				 bapi_tween_manager_destroy(bapi_tween_manager_t manager);
bapi_tween_t bapi_tween_create(bapi_tween_manager_t manager, float from, float to,
							   float duration, bapi_ease_t ease, int loop);
void		 bapi_tween_destroy(bapi_tween_manager_t manager, bapi_tween_t tween);
void		 bapi_tween_manager_update(bapi_tween_manager_t manager, float delta_time);
float		 bapi_tween_value(bapi_tween_t tween);
int			 bapi_tween_is_finished(bapi_tween_t tween);

/* --- Particles -----------------------------------------------------------
 * Fixed-capacity particle pool. Particles are drawn as colored squares; no
 * texture or platform-specific state is required.
 */
typedef struct bapi_particle_system_internal *bapi_particle_system_t;

typedef struct {
	bapi_vec2_t	 position;
	bapi_vec2_t	 velocity_min;
	bapi_vec2_t	 velocity_max;
	bapi_color_t color_start;
	bapi_color_t color_end;
	float		 life_min;
	float		 life_max;
	float		 size_min;
	float		 size_max;
	float		 gravity;
	float		 emission_rate;
} bapi_particle_config_t;

bapi_particle_system_t bapi_particle_system_create(int max_particles);
void				   bapi_particle_system_destroy(bapi_particle_system_t system);
void bapi_particle_system_set_config(bapi_particle_system_t system,
									 const bapi_particle_config_t *config);
void bapi_particle_system_set_position(bapi_particle_system_t system, float x, float y);
void bapi_particle_system_emit(bapi_particle_system_t system, int count);
void bapi_particle_system_update(bapi_particle_system_t system, float delta_time);
void bapi_particle_system_draw(bapi_particle_system_t system);
int	 bapi_particle_system_count(bapi_particle_system_t system);

/* --- Texture atlas --------------------------------------------------------
 * `bapi_atlas_load()` reads a text manifest (BE_ATLAS 1) and loads its page
 * image. `bapi_atlas_load_with_texture()` uses an already-loaded page and does
 * not take ownership of it. Region names are pointers owned by the atlas and
 * stay valid until bapi_atlas_destroy().
 */
typedef struct bapi_atlas_internal *bapi_atlas_t;

typedef struct {
	const char *name;
	int			 x;
	int			 y;
	int			 w;
	int			 h;
} bapi_atlas_region_t;

bapi_atlas_t				bapi_atlas_load(const char *manifest_path);
bapi_atlas_t				bapi_atlas_load_with_texture(const char *manifest_path,
														  bapi_texture_t page);
bapi_atlas_t				bapi_atlas_load_from_memory(const void *manifest, size_t size,
												bapi_texture_t page);
void						bapi_atlas_destroy(bapi_atlas_t atlas);
bapi_texture_t				bapi_atlas_get_texture(bapi_atlas_t atlas);
int							bapi_atlas_get_region_count(bapi_atlas_t atlas);
const bapi_atlas_region_t *bapi_atlas_get_region(bapi_atlas_t atlas, int index);
const bapi_atlas_region_t *bapi_atlas_find_region(bapi_atlas_t atlas, const char *name);
void bapi_atlas_draw(bapi_atlas_t atlas, const char *name, bapi_rect_t dst, int flip,
					 bapi_color_t tint);

/* --- Sprite batch ---------------------------------------------------------
 * Accumulate atlas sprites sharing one texture into a single geometry call.
 * Call begin() once, draw() many times, then end() to flush. A texture change
 * flushes automatically; a full batch flushes automatically. Do not interleave
 * other draw/text calls with an active batch. Platforms without textured
 * geometry fall back to immediate bapi_texture_draw() calls.
 */
void bapi_sprite_batch_begin(void);
void bapi_sprite_batch_draw(bapi_texture_t texture, const bapi_texture_draw_t *params);
void bapi_sprite_batch_end(void);
/* Decode image bytes held in memory. The input buffer is only read during the
 * call and is not kept. */
bapi_texture_t bapi_texture_load_from_memory(const void *data, size_t size);

int			 bapi_video_init(void);
void		 bapi_video_cleanup(void);
bapi_video_t bapi_video_load(const char *filepath);
void		 bapi_video_free(bapi_video_t video);
int			 bapi_video_play(bapi_video_t video);
void		 bapi_video_pause(bapi_video_t video);
void		 bapi_video_stop(bapi_video_t video);
void		 bapi_video_render(bapi_video_t video, int x, int y, int w, int h);
void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h);
void bapi_video_render_center(bapi_video_t video, int window_w, int window_h);
void bapi_video_set_loop(bapi_video_t video, int loop);
void bapi_video_set_volume(bapi_video_t video, float volume);
int	 bapi_video_is_playing(bapi_video_t video);
void bapi_video_get_size(bapi_video_t video, int *w, int *h);
void bapi_video_update(void);
/* Load video from memory: the bytes are copied into video-owned storage and
 * freed by bapi_video_free(). The caller's buffer may be released right after
 * the call. */
bapi_video_t bapi_video_load_from_memory(const void *data, size_t size);
/* Load video from a pack entry in streaming mode (no full-file buffer): the
 * stream is owned by the returned video and closed by bapi_video_free().
 * The pack must stay open until the video is freed. */
bapi_video_t bapi_video_load_from_pack_stream(bapi_pack_t pack, const char *name);

bapi_scene_manager_t bapi_scene_manager_load_from_xml(const char *filepath);
bapi_level_manager_t bapi_level_manager_load_from_xml(const char *filepath);
int bapi_scene_manager_save_to_xml(bapi_scene_manager_t manager, const char *filepath);
int bapi_level_manager_save_to_xml(bapi_level_manager_t manager, const char *filepath);

bapi_ui_t				 bapi_ui_create(void);
bapi_ui_t				 bapi_ui_load_from_xml(const char *filepath);
// Load/save a UI document with an optional encryption key.
//
// If `key` is NULL or empty the file is plain XML (compatible with the
// load_from_xml/save_to_xml functions). If `key` is non-empty the file is
// stored as a binary .uix document: the XML payload is obfuscated with the key
// and a header magic + checksum is written, so the file cannot be edited by
// hand or trivially read. Loading a .uix file with a wrong/empty key fails and
// returns NULL. Loading a plain XML file with a key also fails (returns NULL)
// rather than silently accepting unencrypted content, so a project that
// requires encryption can detect tampering. bapi_ui_load_from_file also
// accepts plain XML when key is empty.
bapi_ui_t bapi_ui_load_from_file(const char *filepath, const char *key);
int		 bapi_ui_save_to_file(bapi_ui_t ui, const char *filepath, const char *key);
// Build-time .uix encrypt/decrypt utilities. Encrypt a plain XML file into a
// binary .uix document, or decrypt one back to plain XML. Intended for the
// build step: the repository keeps editable plain XML, and the build encrypts
// a copy into the output directory. Returns 0 on success, -1 on failure.
int bapi_uix_encrypt_file(const char *src_xml_path, const char *dst_uix_path, const char *key);
int bapi_uix_decrypt_file(const char *src_uix_path, const char *dst_xml_path, const char *key);
void	 bapi_ui_destroy(bapi_ui_t ui);
void					 bapi_ui_update(bapi_ui_t ui, const bapi_event_t *event);
void					 bapi_ui_render(bapi_ui_t ui);
void					 bapi_ui_layout(bapi_ui_t ui);
int						 bapi_ui_was_clicked(bapi_ui_t ui, const char *id);
bapi_ui_component_t		 bapi_ui_find(bapi_ui_t ui, const char *id);
bapi_ui_component_type_t bapi_ui_component_get_type(bapi_ui_component_t component);
void		bapi_ui_component_get_rect(bapi_ui_component_t component, bapi_rect_t *out_rect);
int			bapi_ui_component_is_visible(bapi_ui_component_t component);
void		bapi_ui_component_set_visible(bapi_ui_component_t component, int visible);
int			bapi_ui_component_is_enabled(bapi_ui_component_t component);
void		bapi_ui_component_set_enabled(bapi_ui_component_t component, int enabled);
const char *bapi_ui_component_get_text(bapi_ui_component_t component);
int			bapi_ui_component_set_text(bapi_ui_component_t component, const char *text);
const char *bapi_ui_component_get_src(bapi_ui_component_t component);
int			bapi_ui_component_set_src(bapi_ui_component_t component, const char *src);
float		bapi_ui_component_get_value(bapi_ui_component_t component);
int			bapi_ui_component_set_value(bapi_ui_component_t component, float value);
int			bapi_ui_component_is_checked(bapi_ui_component_t component);
int			bapi_ui_component_is_focused(bapi_ui_component_t component);
int			bapi_ui_component_get_selected_index(bapi_ui_component_t component);
int			bapi_ui_component_set_selected_index(bapi_ui_component_t component, int index);
float		bapi_ui_component_get_scroll_offset(bapi_ui_component_t component);
int			bapi_ui_component_set_scroll_offset(bapi_ui_component_t component, float offset);
int			bapi_ui_save_to_xml(bapi_ui_t ui, const char *filepath);
void		bapi_ui_render_ex(bapi_ui_t ui, float offset_x, float offset_y, float scale);

bapi_ui_component_t bapi_ui_component_create(bapi_ui_component_type_t type, const char *id);
void				bapi_ui_component_destroy(bapi_ui_component_t component);
bapi_ui_component_t bapi_ui_component_clone(bapi_ui_component_t source);
int bapi_ui_component_add_child(bapi_ui_component_t parent, bapi_ui_component_t child);
int bapi_ui_component_insert_child(bapi_ui_component_t parent, bapi_ui_component_t child, int index);
int bapi_ui_component_remove(bapi_ui_component_t component);
int bapi_ui_add_root(bapi_ui_t ui, bapi_ui_component_t component);
int bapi_ui_remove_root(bapi_ui_t ui, bapi_ui_component_t component);
int bapi_ui_insert_root(bapi_ui_t ui, bapi_ui_component_t component, int index);

bapi_ui_component_t bapi_ui_component_get_parent(bapi_ui_component_t component);
int					bapi_ui_component_get_child_count(bapi_ui_component_t component);
bapi_ui_component_t bapi_ui_component_get_child(bapi_ui_component_t component, int index);

int					 bapi_ui_get_root_count(bapi_ui_t ui);
bapi_ui_component_t	 bapi_ui_get_root(bapi_ui_t ui, int index);

void bapi_ui_component_set_rect(bapi_ui_component_t component, bapi_rect_t rect);
const char *bapi_ui_component_get_id(bapi_ui_component_t component);
int	 bapi_ui_component_set_id(bapi_ui_component_t component, const char *id);
int	 bapi_ui_component_set_color(bapi_ui_component_t component, bapi_ui_color_role_t role,
							   bapi_color_t color);
int	 bapi_ui_component_get_color(bapi_ui_component_t component, bapi_ui_color_role_t role,
							   bapi_color_t *out);
void bapi_ui_component_set_text_size(bapi_ui_component_t component, float size);
float bapi_ui_component_get_text_size(bapi_ui_component_t component);

float bapi_ui_component_get_min_value(bapi_ui_component_t component);
int	 bapi_ui_component_set_min_value(bapi_ui_component_t component, float value);
float bapi_ui_component_get_max_value(bapi_ui_component_t component);
int	 bapi_ui_component_set_max_value(bapi_ui_component_t component, float value);
float bapi_ui_component_get_step(bapi_ui_component_t component);
int	 bapi_ui_component_set_step(bapi_ui_component_t component, float step);
int	 bapi_ui_component_set_checked(bapi_ui_component_t component, int checked);
int	 bapi_ui_component_get_columns(bapi_ui_component_t component);
int	 bapi_ui_component_set_columns(bapi_ui_component_t component, int columns);
float bapi_ui_component_get_radius(bapi_ui_component_t component);
int	 bapi_ui_component_set_radius(bapi_ui_component_t component, float radius);
int	 bapi_ui_component_get_sides(bapi_ui_component_t component);
int	 bapi_ui_component_set_sides(bapi_ui_component_t component, int sides);
int	 bapi_ui_component_get_relative(bapi_ui_component_t component);
int	 bapi_ui_component_set_relative(bapi_ui_component_t component, int relative);

// --- Project (.bep) loading --------------------------------------------------
// .bep is the editor's project file format (INI-style: name / engine /
// [documents] section with relative document paths). These functions let a
// runtime binary load a project description at startup without hard-coding the
// list of scene documents. All returned strings are owned by the project and
// remain valid until bapi_project_destroy.
bapi_project_t bapi_project_load_from_bep(const char *filepath);
void		   bapi_project_destroy(bapi_project_t project);
const char	  *bapi_project_get_name(bapi_project_t project);
const char	  *bapi_project_get_engine_dir(bapi_project_t project);
int			   bapi_project_get_document_count(bapi_project_t project);
const char	  *bapi_project_get_document_path(bapi_project_t project, int index);
bapi_project_t bapi_project_create(const char *name, const char *engine_dir);
int			   bapi_project_save(bapi_project_t project, const char *filepath);
int			   bapi_project_add_document(bapi_project_t project, const char *path);
int			   bapi_project_remove_document(bapi_project_t project, const char *path);
int			   bapi_project_set_engine(bapi_project_t project, const char *engine_dir);

const char *bridgeengine_get_version(void);
int			bridgeengine_get_version_number(void);

bapi_file_t bapi_file_open(const char *path);
size_t		bapi_file_read(bapi_file_t file, void *buffer, size_t size);
int64_t		bapi_file_seek(bapi_file_t file, int64_t offset, int origin);
int64_t		bapi_file_tell(bapi_file_t file);
int64_t		bapi_file_size(bapi_file_t file);
void		bapi_file_close(bapi_file_t file);
/* Read a whole file into a malloc'd buffer; the caller frees it. Returns NULL
 * on failure, in which case *out_size is left untouched. An empty file
 * returns a non-NULL pointer with *out_size == 0. */
uint8_t		*bapi_file_read_alloc(const char *path, size_t *out_size);

/* Resource pack (RZip .rz), read-only. Single-threaded use; the returned name
 * pointers and the handle stay valid until bapi_pack_close(). */
bapi_pack_t bapi_pack_open(const char *path);
void		bapi_pack_close(bapi_pack_t pack);
int			bapi_pack_file_count(bapi_pack_t pack);
const char *bapi_pack_file_name(bapi_pack_t pack, int index);
int			bapi_pack_find_file(bapi_pack_t pack, const char *name);
int64_t		bapi_pack_file_size(bapi_pack_t pack, const char *name);
size_t		bapi_pack_read_file(bapi_pack_t pack, const char *name, void *buffer,
								size_t buffer_size);
uint8_t		*bapi_pack_read_file_alloc(bapi_pack_t pack, const char *name, size_t *out_size);

/* Streaming read of a pack entry. The stream borrows the pack: the pack must
 * stay open until the stream is closed. seek() takes SEEK_SET/SEEK_CUR/SEEK_END
 * (0/1/2) and clamps the position into [0, size]. */
bapi_pack_stream_t bapi_pack_stream_open(bapi_pack_t pack, const char *name);
size_t		bapi_pack_stream_read(bapi_pack_stream_t stream, void *buffer, size_t size);
int64_t		bapi_pack_stream_seek(bapi_pack_stream_t stream, int64_t offset, int whence);
int64_t		bapi_pack_stream_tell(bapi_pack_stream_t stream);
int64_t		bapi_pack_stream_size(bapi_pack_stream_t stream);
void		bapi_pack_stream_close(bapi_pack_stream_t stream);

/* Load media from a pack entry via a whole-entry memory buffer (full-buffer
 * set); see bapi_*_load_from_memory for ownership of the decoded resource. */
bapi_sound_t	bapi_sound_load_from_pack(bapi_pack_t pack, const char *name);
bapi_texture_t	bapi_texture_load_from_pack(bapi_pack_t pack, const char *name);
bapi_video_t	bapi_video_load_from_pack(bapi_pack_t pack, const char *name);

#ifdef __cplusplus
}
#endif

#if defined(BAPI_LOG_ENABLED)
#define BAPI_LOG_DEBUG(fmt, ...)                                                                   \
	bapi_log_message(BAPI_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define BAPI_LOG_INFO(fmt, ...)                                                                    \
	bapi_log_message(BAPI_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define BAPI_LOG_WARN(fmt, ...)                                                                    \
	bapi_log_message(BAPI_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define BAPI_LOG_ERROR(fmt, ...)                                                                   \
	bapi_log_message(BAPI_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define BAPI_LOG_CRITICAL(fmt, ...)                                                                \
	bapi_log_message(BAPI_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define BAPI_LOG_ASSERT(condition, fmt, ...)                                                       \
	do {                                                                                           \
		if (!(condition))                                                                          \
			bapi_log_message(BAPI_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __func__,                \
							 "ASSERTION FAILED: " fmt, ##__VA_ARGS__);                             \
	} while (0)
#else
#define BAPI_LOG_DEBUG(fmt, ...)			 ((void)0)
#define BAPI_LOG_INFO(fmt, ...)				 ((void)0)
#define BAPI_LOG_WARN(fmt, ...)				 ((void)0)
#define BAPI_LOG_ERROR(fmt, ...)			 ((void)0)
#define BAPI_LOG_CRITICAL(fmt, ...)			 ((void)0)
#define BAPI_LOG_ASSERT(condition, fmt, ...) ((void)(condition))
#endif

#define BAPI_LOG_INIT_DEFAULT()                                                                    \
	do {                                                                                           \
		bapi_log_config_t config = {BAPI_LOG_LEVEL_INFO, true, false, NULL};                       \
		bapi_log_init(&config);                                                                    \
	} while (0)
