/*
 * Optional SDL_GPU renderer for the SDL3 backend.
 *
 * The GPU device is requested with the "vulkan" driver. SDL_GPU handles the
 * Vulkan 1.0 + maintenance1 details, format fallbacks and swapchain
 * management; the engine falls back to SDL_Renderer when this fails.
 */

#include "internal/platform/sdl3_internal.h"
#include "sdlgpu_shaders.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SDLGPU_MAX_VERTICES 262144
#define SDLGPU_MAX_RANGES	4096

typedef struct {
	float x;
	float y;
	float u;
	float v;
	float r;
	float g;
	float b;
	float a;
} sdlgpu_vertex_t;

typedef struct {
	SDL_GPUTexture *texture;
	int				first;
	int				count;
} sdlgpu_range_t;

struct sdlgpu_texture {
	struct sdlgpu_context *context;
	SDL_GPUDevice	   *device;
	SDL_GPUTexture	   *texture;
	int					width;
	int					height;
	SDL_GPUTextureFormat format;
};

struct sdlgpu_context {
	SDL_GPUDevice			*device;
	SDL_Window				*window;
	SDL_GPUTextureFormat	 swapchain_format;
	SDL_GPUShader			*vertex_shader;
	SDL_GPUShader			*fragment_shader;
	SDL_GPUGraphicsPipeline *pipeline;
	SDL_GPUSampler			*sampler;
	SDL_GPUTexture			*white_texture;
	SDL_GPUBuffer			*vertex_buffer;
	SDL_GPUTransferBuffer *pending_transfers[128];
	int					  pending_transfer_count;
	SDL_GPUCommandBuffer	*command_buffer;
	SDL_GPURenderPass		*render_pass;
	SDL_GPUTexture			*swapchain_texture;
	Uint32					 swapchain_width;
	Uint32					 swapchain_height;

	sdlgpu_vertex_t *vertices;
	int				 vertex_count;
	sdlgpu_range_t	 ranges[SDLGPU_MAX_RANGES];
	int				 range_count;
	int				 range_start;
	SDL_GPUTexture	*current_texture;

	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	uint8_t clear_r;
	uint8_t clear_g;
	uint8_t clear_b;
	uint8_t clear_a;
	int		logical_width;
	int		logical_height;
	int		vsync;
	int		in_frame;
};

static float sdlgpu_min(float a, float b)
{
	return a < b ? a : b;
}

static void sdlgpu_range_end(struct sdlgpu_context *gpu)
{
	int count = gpu->vertex_count - gpu->range_start;
	if (count <= 0 || gpu->current_texture == NULL) return;
	if (gpu->range_count >= SDLGPU_MAX_RANGES) return;
	sdlgpu_range_t *range = &gpu->ranges[gpu->range_count++];
	range->texture		  = gpu->current_texture;
	range->count		  = count;
	range->first		  = gpu->range_start;
	gpu->range_start	  = gpu->vertex_count;
}

static void sdlgpu_use_texture(struct sdlgpu_context *gpu, SDL_GPUTexture *texture)
{
	if (texture == NULL) texture = gpu->white_texture;
	if (gpu->current_texture == texture) return;
	if (gpu->current_texture != NULL) sdlgpu_range_end(gpu);
	gpu->current_texture = texture;
}

static void sdlgpu_emit(struct sdlgpu_context *gpu, float x, float y, float u, float v, float r,
						float g, float b, float a)
{
	if (gpu->vertex_count >= SDLGPU_MAX_VERTICES) return;

	float scale_x = 1.0f, scale_y = 1.0f, offset_x = 0.0f, offset_y = 0.0f;
	if (gpu->logical_width > 0 && gpu->logical_height > 0 && gpu->swapchain_width > 0 &&
		gpu->swapchain_height > 0) {
		float scale = sdlgpu_min((float)gpu->swapchain_width / (float)gpu->logical_width,
								 (float)gpu->swapchain_height / (float)gpu->logical_height);
		scale_x = scale_y = scale;
		offset_x = ((float)gpu->swapchain_width - (float)gpu->logical_width * scale) * 0.5f;
		offset_y = ((float)gpu->swapchain_height - (float)gpu->logical_height * scale) * 0.5f;
	}

	sdlgpu_vertex_t *vertex = &gpu->vertices[gpu->vertex_count++];
	vertex->x				 = ((x * scale_x + offset_x) / (float)gpu->swapchain_width) * 2.0f - 1.0f;
	vertex->y				 = 1.0f - ((y * scale_y + offset_y) / (float)gpu->swapchain_height) * 2.0f;
	vertex->u				 = u;
	vertex->v				 = v;
	vertex->r				 = r;
	vertex->g				 = g;
	vertex->b				 = b;
	vertex->a				 = a;
}

static void sdlgpu_emit_triangle(struct sdlgpu_context *gpu, float x0, float y0, float u0, float v0,
								 float x1, float y1, float u1, float v1, float x2, float y2,
								 float u2, float v2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	sdlgpu_emit(gpu, x0, y0, u0, v0, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
	sdlgpu_emit(gpu, x1, y1, u1, v1, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
	sdlgpu_emit(gpu, x2, y2, u2, v2, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

static void sdlgpu_emit_quad(struct sdlgpu_context *gpu, float x0, float y0, float x1, float y1,
							 float x2, float y2, float x3, float y3, float u0, float v0, float u1,
							 float v1, float u2, float v2, float u3, float v3, uint8_t r, uint8_t g,
							 uint8_t b, uint8_t a)
{
	sdlgpu_emit_triangle(gpu, x0, y0, u0, v0, x1, y1, u1, v1, x2, y2, u2, v2, r, g, b, a);
	sdlgpu_emit_triangle(gpu, x0, y0, u0, v0, x2, y2, u2, v2, x3, y3, u3, v3, r, g, b, a);
}

static void sdlgpu_begin_frame(struct sdlgpu_context *gpu)
{
	if (gpu->in_frame) return;

	gpu->command_buffer = SDL_AcquireGPUCommandBuffer(gpu->device);
	if (gpu->command_buffer == NULL) return;

	if (!SDL_WaitAndAcquireGPUSwapchainTexture(gpu->command_buffer, gpu->window,
											   &gpu->swapchain_texture, &gpu->swapchain_width,
											   &gpu->swapchain_height) ||
		gpu->swapchain_texture == NULL) {
		SDL_CancelGPUCommandBuffer(gpu->command_buffer);
		gpu->command_buffer = NULL;
		return;
	}

	gpu->vertex_count	 = 0;
	gpu->range_count	 = 0;
	gpu->range_start	 = 0;
	gpu->current_texture = NULL;
	gpu->in_frame		 = 1;
}

static int sdlgpu_create_pipeline(struct sdlgpu_context *gpu)
{
	SDL_GPUShaderCreateInfo vertex_info;
	SDL_zero(vertex_info);
	vertex_info.code			 = sdlgpu_vert_spv;
	vertex_info.code_size		 = sdlgpu_vert_spv_len;
	vertex_info.entrypoint		 = "main";
	vertex_info.format			 = SDL_GPU_SHADERFORMAT_SPIRV;
	vertex_info.stage			 = SDL_GPU_SHADERSTAGE_VERTEX;
	vertex_info.num_samplers	 = 0;
	vertex_info.num_uniform_buffers = 0;

	SDL_GPUShaderCreateInfo fragment_info;
	SDL_zero(fragment_info);
	fragment_info.code			   = sdlgpu_frag_spv;
	fragment_info.code_size		   = sdlgpu_frag_spv_len;
	fragment_info.entrypoint	   = "main";
	fragment_info.format		   = SDL_GPU_SHADERFORMAT_SPIRV;
	fragment_info.stage			   = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_samplers	   = 1;
	fragment_info.num_uniform_buffers = 0;

	gpu->vertex_shader	 = SDL_CreateGPUShader(gpu->device, &vertex_info);
	gpu->fragment_shader = SDL_CreateGPUShader(gpu->device, &fragment_info);
	if (gpu->vertex_shader == NULL || gpu->fragment_shader == NULL) return -1;

	SDL_GPUVertexBufferDescription buffer_description;
	SDL_zero(buffer_description);
	buffer_description.slot			 = 0;
	buffer_description.pitch		 = sizeof(sdlgpu_vertex_t);
	buffer_description.input_rate	 = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	buffer_description.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[3];
	SDL_zero(attributes);
	attributes[0].location	  = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format	  = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[0].offset	  = 0;
	attributes[1].location	  = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format	  = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[1].offset	  = 8;
	attributes[2].location	  = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format	  = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[2].offset	  = 16;

	SDL_GPUVertexInputState vertex_input;
	SDL_zero(vertex_input);
	vertex_input.vertex_buffer_descriptions = &buffer_description;
	vertex_input.num_vertex_buffers		   = 1;
	vertex_input.vertex_attributes		   = attributes;
	vertex_input.num_vertex_attributes	   = 3;

	SDL_GPURasterizerState rasterizer;
	SDL_zero(rasterizer);
	rasterizer.fill_mode		 = SDL_GPU_FILLMODE_FILL;
	rasterizer.cull_mode		 = SDL_GPU_CULLMODE_NONE;
	rasterizer.front_face		 = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	rasterizer.enable_depth_clip = true;

	SDL_GPUMultisampleState multisample;
	SDL_zero(multisample);
	multisample.sample_count = SDL_GPU_SAMPLECOUNT_1;

	SDL_GPUColorTargetBlendState blend;
	SDL_zero(blend);
	blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	blend.color_blend_op		= SDL_GPU_BLENDOP_ADD;
	blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alpha_blend_op		= SDL_GPU_BLENDOP_ADD;
	blend.enable_blend			= true;

	SDL_GPUColorTargetDescription color_target;
	SDL_zero(color_target);
	color_target.format		 = gpu->swapchain_format;
	color_target.blend_state = blend;

	SDL_GPUGraphicsPipelineTargetInfo target_info;
	SDL_zero(target_info);
	target_info.color_target_descriptions = &color_target;
	target_info.num_color_targets		  = 1;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
	SDL_zero(pipeline_info);
	pipeline_info.vertex_shader		 = gpu->vertex_shader;
	pipeline_info.fragment_shader	 = gpu->fragment_shader;
	pipeline_info.vertex_input_state = vertex_input;
	pipeline_info.primitive_type	 = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state	 = rasterizer;
	pipeline_info.multisample_state	 = multisample;
	pipeline_info.target_info		 = target_info;

	gpu->pipeline = SDL_CreateGPUGraphicsPipeline(gpu->device, &pipeline_info);
	return gpu->pipeline != NULL ? 0 : -1;
}

static plat_texture_t sdlgpu_make_texture(struct sdlgpu_context *gpu, int width, int height,
										  SDL_GPUTextureFormat format)
{
	struct sdlgpu_texture *texture = calloc(1, sizeof(*texture));
	if (texture == NULL) return NULL;

	SDL_GPUTextureCreateInfo info;
	SDL_zero(info);
	info.type				 = SDL_GPU_TEXTURETYPE_2D;
	info.format				 = format;
	info.usage				 = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	info.width				 = (Uint32)width;
	info.height				 = (Uint32)height;
	info.layer_count_or_depth = 1;
	info.num_levels			 = 1;
	info.sample_count		 = SDL_GPU_SAMPLECOUNT_1;

	texture->texture = SDL_CreateGPUTexture(gpu->device, &info);
	if (texture->texture == NULL) {
		free(texture);
		return NULL;
	}
	texture->context = gpu;
	texture->device = gpu->device;
	texture->width	= width;
	texture->height = height;
	texture->format = format;

	plat_texture_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		SDL_ReleaseGPUTexture(gpu->device, texture->texture);
		free(texture);
		return NULL;
	}
	result->gpu	   = texture;
	result->width  = width;
	result->height = height;
	return result;
}

static int sdlgpu_upload(struct sdlgpu_texture *texture, const void *pixels, int pitch, int width,
						 int height)
{
	struct sdlgpu_context *gpu = texture->context;
	SDL_GPUDevice *device = texture->device;
	Uint32 row_bytes = (Uint32)(pitch > 0 ? pitch : width * 4);
	Uint32 size		 = row_bytes * (Uint32)height;
	if (texture->format == SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM ||
		texture->format == SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM) {
		Uint32 block_size = texture->format == SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM ? 8 : 16;
		Uint32 blocks_x = ((Uint32)width + 3) / 4;
		Uint32 blocks_y = ((Uint32)height + 3) / 4;
		row_bytes = blocks_x * block_size;
		size = row_bytes * blocks_y;
	}

	SDL_GPUTransferBufferCreateInfo transfer_info;
	SDL_zero(transfer_info);
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_info.size	= size;
	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	if (transfer == NULL) return -1;

	void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
	if (mapped == NULL) {
		SDL_ReleaseGPUTransferBuffer(device, transfer);
		return -1;
	}
	memcpy(mapped, pixels, size);
	SDL_UnmapGPUTransferBuffer(device, transfer);

	SDL_GPUCommandBuffer *command = NULL;
	int in_frame = gpu != NULL && gpu->in_frame && gpu->command_buffer != NULL;
	if (in_frame) {
		command = gpu->command_buffer;
	} else {
		command = SDL_AcquireGPUCommandBuffer(device);
		if (command == NULL) {
			SDL_ReleaseGPUTransferBuffer(device, transfer);
			return -1;
		}
	}

	SDL_GPUTextureTransferInfo source;
	SDL_zero(source);
	source.transfer_buffer = transfer;
	source.offset		   = 0;
	source.pixels_per_row  = (texture->format == SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM ||
							  texture->format == SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM)
							 ? 0
							 : (Uint32)(row_bytes / 4);
	source.rows_per_layer  = (texture->format == SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM ||
							  texture->format == SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM)
							 ? 0
							 : (Uint32)height;

	SDL_GPUTextureRegion region;
	SDL_zero(region);
	region.texture = texture->texture;
	region.w	   = (Uint32)width;
	region.h	   = (Uint32)height;
	region.d	   = 1;

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command);
	SDL_UploadToGPUTexture(copy_pass, &source, &region, false);
	SDL_EndGPUCopyPass(copy_pass);
	if (in_frame) {
		if (gpu->pending_transfer_count < (int)(sizeof(gpu->pending_transfers) /
											   sizeof(gpu->pending_transfers[0]))) {
			gpu->pending_transfers[gpu->pending_transfer_count++] = transfer;
		} else {
			SDL_ReleaseGPUTransferBuffer(device, transfer);
		}
	} else {
		SDL_SubmitGPUCommandBuffer(command);
		SDL_WaitForGPUIdle(device);
		SDL_ReleaseGPUTransferBuffer(device, transfer);
	}
	return 0;
}

static SDL_GPUTextureFormat sdlgpu_map_format(plat_pixel_format_t format)
{
	switch (format) {
	case PLAT_PIXELFORMAT_ARGB8888: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
	case PLAT_PIXELFORMAT_BC1: return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
	case PLAT_PIXELFORMAT_BC3: return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
	default: return SDL_GPU_TEXTUREFORMAT_INVALID;
	}
}

struct sdlgpu_context *sdlgpu_create(plat_window_t window)
{
	if (window == NULL) return NULL;

	SDL_PropertiesID props = SDL_CreateProperties();
	if (props == 0) return NULL;
	SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,
						   false);
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
	SDL_GPUDevice *device = SDL_CreateGPUDeviceWithProperties(props);
	SDL_DestroyProperties(props);
	if (device == NULL) return NULL;

	if (!SDL_ClaimWindowForGPUDevice(device, window->window)) {
		SDL_DestroyGPUDevice(device);
		return NULL;
	}

	struct sdlgpu_context *gpu = calloc(1, sizeof(*gpu));
	if (gpu == NULL) {
		SDL_DestroyGPUDevice(device);
		return NULL;
	}
	gpu->device			  = device;
	gpu->window			  = window->window;
	gpu->swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window->window);
	gpu->vertices		  = malloc(sizeof(sdlgpu_vertex_t) * SDLGPU_MAX_VERTICES);
	if (gpu->vertices == NULL) {
		SDL_ReleaseWindowFromGPUDevice(device, window->window);
		SDL_DestroyGPUDevice(device);
		free(gpu);
		return NULL;
	}
	gpu->r = 0;
	gpu->g = 0;
	gpu->b = 0;
	gpu->a = 255;
	gpu->clear_r = 0;
	gpu->clear_g = 0;
	gpu->clear_b = 0;
	gpu->clear_a = 255;

	if (sdlgpu_create_pipeline(gpu) != 0) {
		sdlgpu_destroy(gpu);
		return NULL;
	}

	SDL_GPUSamplerCreateInfo sampler_info;
	SDL_zero(sampler_info);
	sampler_info.min_filter	   = SDL_GPU_FILTER_LINEAR;
	sampler_info.mag_filter	   = SDL_GPU_FILTER_LINEAR;
	sampler_info.mipmap_mode   = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	gpu->sampler = SDL_CreateGPUSampler(device, &sampler_info);
	if (gpu->sampler == NULL) {
		sdlgpu_destroy(gpu);
		return NULL;
	}

	SDL_GPUBufferCreateInfo buffer_info;
	SDL_zero(buffer_info);
	buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	buffer_info.size  = (Uint32)(sizeof(sdlgpu_vertex_t) * SDLGPU_MAX_VERTICES);
	gpu->vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
	if (gpu->vertex_buffer == NULL) {
		sdlgpu_destroy(gpu);
		return NULL;
	}

	uint8_t white[4] = {255, 255, 255, 255};
	plat_texture_t white_texture = sdlgpu_make_texture(gpu, 1, 1, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
	if (white_texture == NULL) {
		sdlgpu_destroy(gpu);
		return NULL;
	}
	if (sdlgpu_upload(white_texture->gpu, white, 4, 1, 1) != 0) {
		sdlgpu_destroy_texture(white_texture);
		sdlgpu_destroy(gpu);
		return NULL;
	}
	gpu->white_texture = white_texture->gpu->texture;
	free(white_texture->gpu);
	free(white_texture);

	return gpu;
}

void sdlgpu_destroy(struct sdlgpu_context *gpu)
{
	if (gpu == NULL) return;
	const char *error = NULL;
	(void)error;
	if (gpu->vertex_shader) SDL_ReleaseGPUShader(gpu->device, gpu->vertex_shader);
	if (gpu->fragment_shader) SDL_ReleaseGPUShader(gpu->device, gpu->fragment_shader);
	if (gpu->pipeline) SDL_ReleaseGPUGraphicsPipeline(gpu->device, gpu->pipeline);
	if (gpu->sampler) SDL_ReleaseGPUSampler(gpu->device, gpu->sampler);
	if (gpu->white_texture) SDL_ReleaseGPUTexture(gpu->device, gpu->white_texture);
	if (gpu->vertex_buffer) SDL_ReleaseGPUBuffer(gpu->device, gpu->vertex_buffer);
	if (gpu->window) SDL_ReleaseWindowFromGPUDevice(gpu->device, gpu->window);
	if (gpu->device) SDL_DestroyGPUDevice(gpu->device);
	free(gpu->vertices);
	free(gpu);
}

void sdlgpu_set_color(struct sdlgpu_context *gpu, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (gpu == NULL) return;
	gpu->r = r;
	gpu->g = g;
	gpu->b = b;
	gpu->a = a;
}

void sdlgpu_set_blend(struct sdlgpu_context *gpu, plat_blend_mode_t mode)
{
	(void)gpu;
	(void)mode;
}

void sdlgpu_render_clear(struct sdlgpu_context *gpu)
{
	if (gpu == NULL || gpu->in_frame) return;
	gpu->clear_r = gpu->r;
	gpu->clear_g = gpu->g;
	gpu->clear_b = gpu->b;
	gpu->clear_a = gpu->a;
	sdlgpu_begin_frame(gpu);
}

void sdlgpu_render_present(struct sdlgpu_context *gpu)
{
	if (gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;

	if (gpu->current_texture != NULL) sdlgpu_range_end(gpu);

	Uint32 vertex_bytes = (Uint32)(gpu->vertex_count * sizeof(sdlgpu_vertex_t));
	SDL_GPUTransferBuffer *transfer = NULL;
	if (vertex_bytes > 0) {
		SDL_GPUTransferBufferCreateInfo transfer_info;
		SDL_zero(transfer_info);
		transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transfer_info.size	= vertex_bytes;
		transfer = SDL_CreateGPUTransferBuffer(gpu->device, &transfer_info);
		if (transfer != NULL) {
			void *mapped = SDL_MapGPUTransferBuffer(gpu->device, transfer, false);
			if (mapped != NULL) {
				memcpy(mapped, gpu->vertices, vertex_bytes);
				SDL_UnmapGPUTransferBuffer(gpu->device, transfer);

				SDL_GPUTransferBufferLocation source = {transfer, 0};
				SDL_GPUBufferRegion destination		 = {gpu->vertex_buffer, 0, vertex_bytes};
				SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(gpu->command_buffer);
				SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
				SDL_EndGPUCopyPass(copy_pass);
			}
		}
	}

	SDL_GPUColorTargetInfo target;
	SDL_zero(target);
	target.texture		 = gpu->swapchain_texture;
	target.load_op		 = SDL_GPU_LOADOP_CLEAR;
	target.store_op		 = SDL_GPU_STOREOP_STORE;
	target.clear_color.r = gpu->clear_r / 255.0f;
	target.clear_color.g = gpu->clear_g / 255.0f;
	target.clear_color.b = gpu->clear_b / 255.0f;
	target.clear_color.a = gpu->clear_a / 255.0f;

	SDL_GPURenderPass *render_pass =
		SDL_BeginGPURenderPass(gpu->command_buffer, &target, 1, NULL);
	if (render_pass != NULL) {
		SDL_GPUViewport viewport = {0.0f, 0.0f, (float)gpu->swapchain_width,
									(float)gpu->swapchain_height, 0.0f, 1.0f};
		SDL_SetGPUViewport(render_pass, &viewport);
		SDL_BindGPUGraphicsPipeline(render_pass, gpu->pipeline);

		if (vertex_bytes > 0 && transfer != NULL) {
			SDL_GPUBufferBinding binding = {gpu->vertex_buffer, 0};
			SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);

			for (int i = 0; i < gpu->range_count; i++) {
				SDL_GPUTextureSamplerBinding texture_binding = {gpu->ranges[i].texture,
																gpu->sampler};
				SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);
				SDL_DrawGPUPrimitives(render_pass, (Uint32)gpu->ranges[i].count, 1,
									  (Uint32)gpu->ranges[i].first, 0);
			}
		}
		SDL_EndGPURenderPass(render_pass);
	}

	SDL_SubmitGPUCommandBuffer(gpu->command_buffer);
	gpu->command_buffer = NULL;
	if (transfer != NULL) SDL_ReleaseGPUTransferBuffer(gpu->device, transfer);
	SDL_WaitForGPUIdle(gpu->device);
	for (int i = 0; i < gpu->pending_transfer_count; i++) {
		SDL_ReleaseGPUTransferBuffer(gpu->device, gpu->pending_transfers[i]);
	}
	gpu->pending_transfer_count = 0;

	gpu->in_frame		 = 0;
	gpu->vertex_count	 = 0;
	gpu->range_count	 = 0;
	gpu->range_start	 = 0;
	gpu->current_texture = NULL;
}

void sdlgpu_render_point(struct sdlgpu_context *gpu, float x, float y)
{
	if (gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, gpu->white_texture);
	sdlgpu_emit_quad(gpu, x, y, x + 1, y, x + 1, y + 1, x, y + 1, 0, 0, 0, 0, 0, 0, 0, 0, gpu->r,
					 gpu->g, gpu->b, gpu->a);
}

void sdlgpu_render_line(struct sdlgpu_context *gpu, float x1, float y1, float x2, float y2)
{
	if (gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	float dx = x2 - x1;
	float dy = y2 - y1;
	float length = sqrtf(dx * dx + dy * dy);
	float nx = length > 0.0f ? -dy / length * 0.5f : 0.0f;
	float ny = length > 0.0f ? dx / length * 0.5f : 0.0f;
	sdlgpu_use_texture(gpu, gpu->white_texture);
	sdlgpu_emit_quad(gpu, x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny,
					 0, 0, 0, 0, 0, 0, 0, 0, gpu->r, gpu->g, gpu->b, gpu->a);
}

void sdlgpu_render_rect(struct sdlgpu_context *gpu, float x, float y, float w, float h)
{
	sdlgpu_render_line(gpu, x, y, x + w, y);
	sdlgpu_render_line(gpu, x + w, y, x + w, y + h);
	sdlgpu_render_line(gpu, x + w, y + h, x, y + h);
	sdlgpu_render_line(gpu, x, y + h, x, y);
}

void sdlgpu_render_fill_rect(struct sdlgpu_context *gpu, float x, float y, float w, float h)
{
	if (gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, gpu->white_texture);
	sdlgpu_emit_quad(gpu, x, y, x + w, y, x + w, y + h, x, y + h, 0, 0, 0, 0, 0, 0, 0, 0, gpu->r,
					 gpu->g, gpu->b, gpu->a);
}

void sdlgpu_render_texture(struct sdlgpu_context *gpu, plat_texture_t texture, float x, float y,
						   float w, float h)
{
	if (gpu == NULL || texture == NULL || texture->gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, texture->gpu->texture);
	sdlgpu_emit_quad(gpu, x, y, x + w, y, x + w, y + h, x, y + h, 0, 0, 1, 0, 1, 1, 0, 1, 255, 255,
					 255, 255);
}

void sdlgpu_render_fill_triangle(struct sdlgpu_context *gpu, float x1, float y1, float x2, float y2,
								 float x3, float y3, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (gpu == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, gpu->white_texture);
	sdlgpu_emit_triangle(gpu, x1, y1, 0, 0, x2, y2, 0, 0, x3, y3, 0, 0, r, g, b, a);
}

void sdlgpu_render_texture_ex(struct sdlgpu_context *gpu, plat_texture_t texture,
							  const plat_rectf_t *src, const plat_rectf_t *dst,
							  const plat_pointf_t *pivot, float rotation, int flip, uint8_t r,
							  uint8_t g, uint8_t b, uint8_t a)
{
	if (gpu == NULL || texture == NULL || texture->gpu == NULL || dst == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, texture->gpu->texture);

	float texture_width  = (float)texture->gpu->width;
	float texture_height = (float)texture->gpu->height;
	float u0 = src ? src->x / texture_width : 0.0f;
	float v0 = src ? src->y / texture_height : 0.0f;
	float u1 = src ? (src->x + src->w) / texture_width : 1.0f;
	float v1 = src ? (src->y + src->h) / texture_height : 1.0f;
	if (flip & 1) {
		float tmp = u0;
		u0		  = u1;
		u1		  = tmp;
	}
	if (flip & 2) {
		float tmp = v0;
		v0		  = v1;
		v1		  = tmp;
	}

	float local_x[4] = {dst->x, dst->x + dst->w, dst->x + dst->w, dst->x};
	float local_y[4] = {dst->y, dst->y, dst->y + dst->h, dst->y + dst->h};
	float u[4]		 = {u0, u1, u1, u0};
	float v[4]		 = {v0, v0, v1, v1};
	float pivot_x	 = pivot ? pivot->x : dst->w * 0.5f;
	float pivot_y	 = pivot ? pivot->y : dst->h * 0.5f;
	float c			 = cosf(rotation);
	float s			 = sinf(rotation);

	float world_x[4];
	float world_y[4];
	for (int i = 0; i < 4; i++) {
		float dx = local_x[i] - (dst->x + pivot_x);
		float dy = local_y[i] - (dst->y + pivot_y);
		world_x[i] = dst->x + pivot_x + dx * c - dy * s;
		world_y[i] = dst->y + pivot_y + dx * s + dy * c;
	}

	sdlgpu_emit_quad(gpu, world_x[0], world_y[0], world_x[1], world_y[1], world_x[2], world_y[2],
					 world_x[3], world_y[3], u[0], v[0], u[1], v[1], u[2], v[2], u[3], v[3], r, g,
					 b, a);
}

void sdlgpu_render_geometry_textured(struct sdlgpu_context *gpu, plat_texture_t texture,
									 const plat_vertex_t *vertices, int vertex_count,
									 const int *indices, int index_count)
{
	if (gpu == NULL || texture == NULL || texture->gpu == NULL || vertices == NULL) return;
	if (!gpu->in_frame) sdlgpu_begin_frame(gpu);
	if (!gpu->in_frame) return;
	sdlgpu_use_texture(gpu, texture->gpu->texture);

	if (indices != NULL && index_count > 0) {
		for (int i = 0; i + 2 < index_count; i += 3) {
			const plat_vertex_t *a = &vertices[indices[i]];
			const plat_vertex_t *b = &vertices[indices[i + 1]];
			const plat_vertex_t *c = &vertices[indices[i + 2]];
			sdlgpu_emit_triangle(gpu, a->x, a->y, a->u, a->v, b->x, b->y, b->u, b->v, c->x, c->y,
								 c->u, c->v, a->r, a->g, a->b, a->a);
		}
	} else {
		for (int i = 0; i + 2 < vertex_count; i += 3) {
			const plat_vertex_t *a = &vertices[i];
			const plat_vertex_t *b = &vertices[i + 1];
			const plat_vertex_t *c = &vertices[i + 2];
			sdlgpu_emit_triangle(gpu, a->x, a->y, a->u, a->v, b->x, b->y, b->u, b->v, c->x, c->y,
								 c->u, c->v, a->r, a->g, a->b, a->a);
		}
	}
}

int sdlgpu_set_vsync(struct sdlgpu_context *gpu, int enabled)
{
	if (gpu == NULL) return -1;
	gpu->vsync = enabled != 0;
	return SDL_SetGPUSwapchainParameters(gpu->device, gpu->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
										 enabled ? SDL_GPU_PRESENTMODE_VSYNC
												 : SDL_GPU_PRESENTMODE_IMMEDIATE)
			   ? 0
			   : -1;
}

int sdlgpu_set_logical_size(struct sdlgpu_context *gpu, int width, int height)
{
	if (gpu == NULL) return -1;
	gpu->logical_width	= width > 0 ? width : 0;
	gpu->logical_height = height > 0 ? height : 0;
	return 0;
}

void sdlgpu_window_to_logical(struct sdlgpu_context *gpu, float *x, float *y)
{
	if (gpu == NULL || x == NULL || y == NULL) return;
	if (gpu->logical_width <= 0 || gpu->logical_height <= 0) return;

	int window_width = 0, window_height = 0;
	SDL_GetWindowSize(gpu->window, &window_width, &window_height);
	if (window_width <= 0 || window_height <= 0) return;

	float scale = sdlgpu_min((float)window_width / (float)gpu->logical_width,
							 (float)window_height / (float)gpu->logical_height);
	float offset_x = ((float)window_width - (float)gpu->logical_width * scale) * 0.5f;
	float offset_y = ((float)window_height - (float)gpu->logical_height * scale) * 0.5f;
	*x			   = (*x - offset_x) / scale;
	*y			   = (*y - offset_y) / scale;
}

plat_texture_t sdlgpu_create_texture_from_surface(struct sdlgpu_context *gpu, plat_surface_t *surface)
{
	if (gpu == NULL || surface == NULL) return NULL;
	SDL_Surface *converted = SDL_ConvertSurface(surface->surface, SDL_PIXELFORMAT_ARGB8888);
	if (converted == NULL) return NULL;
	plat_texture_t texture =
		sdlgpu_make_texture(gpu, converted->w, converted->h,
							SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
	if (texture != NULL) {
		sdlgpu_upload(texture->gpu, converted->pixels, converted->pitch, converted->w,
					  converted->h);
	}
	SDL_DestroySurface(converted);
	return texture;
}

plat_texture_t sdlgpu_create_texture(struct sdlgpu_context *gpu, plat_pixel_format_t format,
									 plat_texture_access_t access, int width, int height)
{
	(void)access;
	if (gpu == NULL || width <= 0 || height <= 0) return NULL;
	SDL_GPUTextureFormat gpu_format = sdlgpu_map_format(format);
	if (gpu_format == SDL_GPU_TEXTUREFORMAT_INVALID) return NULL;
	if (!SDL_GPUTextureSupportsFormat(gpu->device, gpu_format, SDL_GPU_TEXTURETYPE_2D,
									  SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
		return NULL;
	}
	return sdlgpu_make_texture(gpu, width, height, gpu_format);
}

int sdlgpu_update_texture(plat_texture_t texture, const void *pixels, int pitch)
{
	if (texture == NULL || texture->gpu == NULL || pixels == NULL) return -1;
	struct sdlgpu_texture *gpu_texture = texture->gpu;
	int					  row_bytes		= pitch > 0 ? pitch : texture->width * 4;
	return sdlgpu_upload(gpu_texture, pixels, row_bytes, texture->width, texture->height);
}

void sdlgpu_destroy_texture(plat_texture_t texture)
{
	if (texture == NULL || texture->gpu == NULL) return;
	if (texture->gpu->texture != NULL) {
		SDL_ReleaseGPUTexture(texture->gpu->device, texture->gpu->texture);
	}
	free(texture->gpu);
	texture->gpu = NULL;
}
