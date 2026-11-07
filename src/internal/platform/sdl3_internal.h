#pragma once

/* Shared SDL3 backend state. Included by platform_sdl3.c and the optional
 * SDL_GPU renderer. Not part of the platform abstraction. */

#include "platform_types.h"

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdlgpu_context;
struct sdlgpu_texture;

struct plat_window {
	SDL_Window *window;
};

struct plat_renderer {
	SDL_Renderer		  *renderer;
	struct sdlgpu_context *gpu;
	uint8_t				   r;
	uint8_t				   g;
	uint8_t				   b;
	uint8_t				   a;
	plat_blend_mode_t	   blend_mode;
};

struct plat_texture {
	SDL_Texture			  *texture;
	struct sdlgpu_texture *gpu;
	int					   width;
	int					   height;
};

struct plat_surface {
	SDL_Surface *surface;
};

/* Renderer backend selection. */
#define SDL3_BACKEND_AUTO	 0
#define SDL3_BACKEND_SDL	 1
#define SDL3_BACKEND_VULKAN 2

void sdl3_set_requested_backend(int backend);
int	 sdl3_get_actual_backend(void);

/* Optional SDL_GPU renderer. All functions return non-zero on failure. */
struct sdlgpu_context *sdlgpu_create(plat_window_t window);
void				   sdlgpu_destroy(struct sdlgpu_context *gpu);
void				   sdlgpu_set_color(struct sdlgpu_context *gpu, uint8_t r, uint8_t g, uint8_t b,
									   uint8_t a);
void sdlgpu_set_blend(struct sdlgpu_context *gpu, plat_blend_mode_t mode);
void sdlgpu_render_clear(struct sdlgpu_context *gpu);
void sdlgpu_render_present(struct sdlgpu_context *gpu);
void sdlgpu_render_point(struct sdlgpu_context *gpu, float x, float y);
void sdlgpu_render_line(struct sdlgpu_context *gpu, float x1, float y1, float x2, float y2);
void sdlgpu_render_rect(struct sdlgpu_context *gpu, float x, float y, float w, float h);
void sdlgpu_render_fill_rect(struct sdlgpu_context *gpu, float x, float y, float w, float h);
void sdlgpu_render_texture(struct sdlgpu_context *gpu, plat_texture_t texture, float x, float y,
						   float w, float h);
void sdlgpu_render_fill_triangle(struct sdlgpu_context *gpu, float x1, float y1, float x2, float y2,
								 float x3, float y3, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void sdlgpu_render_texture_ex(struct sdlgpu_context *gpu, plat_texture_t texture,
							  const plat_rectf_t *src, const plat_rectf_t *dst,
							  const plat_pointf_t *pivot, float rotation, int flip, uint8_t r,
							  uint8_t g, uint8_t b, uint8_t a);
void sdlgpu_render_geometry_textured(struct sdlgpu_context *gpu, plat_texture_t texture,
									 const plat_vertex_t *vertices, int vertex_count,
									 const int *indices, int index_count);
int	 sdlgpu_set_vsync(struct sdlgpu_context *gpu, int enabled);
int	 sdlgpu_set_logical_size(struct sdlgpu_context *gpu, int width, int height);
void sdlgpu_window_to_logical(struct sdlgpu_context *gpu, float *x, float *y);

plat_texture_t sdlgpu_create_texture_from_surface(struct sdlgpu_context *gpu, plat_surface_t *surface);
plat_texture_t sdlgpu_create_texture(struct sdlgpu_context *gpu, plat_pixel_format_t format,
									 plat_texture_access_t access, int width, int height);
int			   sdlgpu_update_texture(plat_texture_t texture, const void *pixels, int pitch);
void		   sdlgpu_destroy_texture(plat_texture_t texture);

#ifdef __cplusplus
}
#endif
