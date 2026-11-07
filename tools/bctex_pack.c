/*
 * Build-time BC1/BC3 texture packer.
 *
 * Usage:
 *   bridgeengine_bctex_pack -f bc1|bc3 -o OUT.bctex IMAGE
 *
 * Output is the BCTX container consumed by bapi_texture_load(). The encoder is
 * intentionally simple (4x4 endpoint min/max + nearest palette index); it is
 * meant for atlas pages and UI textures, not for high-quality normal maps.
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BCTX_HEADER_SIZE 20

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
	return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void rgb565_to_rgb(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
	*r = (uint8_t)(((color >> 11) & 0x1F) * 255 / 31);
	*g = (uint8_t)(((color >> 5) & 0x3F) * 255 / 63);
	*b = (uint8_t)((color & 0x1F) * 255 / 31);
}

static int color_distance(uint8_t r, uint8_t g, uint8_t b, uint8_t r2, uint8_t g2, uint8_t b2)
{
	int dr = (int)r - (int)r2;
	int dg = (int)g - (int)g2;
	int db = (int)b - (int)b2;
	return dr * dr + dg * dg + db * db;
}

static void encode_color_block(const uint8_t pixels[16][4], uint8_t output[8])
{
	uint8_t min_r = 255, min_g = 255, min_b = 255;
	uint8_t max_r = 0, max_g = 0, max_b = 0;
	for (int i = 0; i < 16; i++) {
		if (pixels[i][0] < min_r) min_r = pixels[i][0];
		if (pixels[i][1] < min_g) min_g = pixels[i][1];
		if (pixels[i][2] < min_b) min_b = pixels[i][2];
		if (pixels[i][0] > max_r) max_r = pixels[i][0];
		if (pixels[i][1] > max_g) max_g = pixels[i][1];
		if (pixels[i][2] > max_b) max_b = pixels[i][2];
	}
	uint16_t c0 = rgb565(max_r, max_g, max_b);
	uint16_t c1 = rgb565(min_r, min_g, min_b);
	if (c0 < c1) {
		uint16_t tmp = c0;
		c0			 = c1;
		c1			 = tmp;
	}
	if (c0 == c1) {
		if (c1 > 0) c1--;
		else c0++;
	}

	uint8_t palette[4][3];
	rgb565_to_rgb(c0, &palette[0][0], &palette[0][1], &palette[0][2]);
	rgb565_to_rgb(c1, &palette[1][0], &palette[1][1], &palette[1][2]);
	for (int i = 0; i < 3; i++) {
		palette[2][i] = (uint8_t)((2 * palette[0][i] + palette[1][i]) / 3);
		palette[3][i] = (uint8_t)((palette[0][i] + 2 * palette[1][i]) / 3);
	}

	output[0] = (uint8_t)(c0 & 0xFF);
	output[1] = (uint8_t)(c0 >> 8);
	output[2] = (uint8_t)(c1 & 0xFF);
	output[3] = (uint8_t)(c1 >> 8);

	uint32_t indices = 0;
	for (int i = 0; i < 16; i++) {
		int best	  = 0;
		int best_dist = color_distance(pixels[i][0], pixels[i][1], pixels[i][2], palette[0][0],
									  palette[0][1], palette[0][2]);
		for (int p = 1; p < 4; p++) {
			int distance = color_distance(pixels[i][0], pixels[i][1], pixels[i][2], palette[p][0],
										  palette[p][1], palette[p][2]);
			if (distance < best_dist) {
				best_dist = distance;
				best	  = p;
			}
		}
		indices |= (uint32_t)best << (2 * i);
	}
	output[4] = (uint8_t)(indices & 0xFF);
	output[5] = (uint8_t)((indices >> 8) & 0xFF);
	output[6] = (uint8_t)((indices >> 16) & 0xFF);
	output[7] = (uint8_t)((indices >> 24) & 0xFF);
}

static void encode_alpha_block(const uint8_t pixels[16][4], uint8_t output[8])
{
	uint8_t min_a = 255, max_a = 0;
	for (int i = 0; i < 16; i++) {
		if (pixels[i][3] < min_a) min_a = pixels[i][3];
		if (pixels[i][3] > max_a) max_a = pixels[i][3];
	}
	uint8_t a0 = max_a;
	uint8_t a1 = min_a;
	if (a0 == a1) {
		if (a1 > 0) a1--;
		else a0++;
	}
	uint8_t palette[8];
	palette[0] = a0;
	palette[1] = a1;
	for (int i = 1; i <= 6; i++) palette[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);

	uint64_t bits = 0;
	for (int i = 0; i < 16; i++) {
		int best	  = 0;
		int best_dist = abs((int)pixels[i][3] - (int)palette[0]);
		for (int p = 1; p < 8; p++) {
			int distance = abs((int)pixels[i][3] - (int)palette[p]);
			if (distance < best_dist) {
				best_dist = distance;
				best	  = p;
			}
		}
		bits |= (uint64_t)best << (3 * i);
	}
	output[0] = a0;
	output[1] = a1;
	for (int i = 0; i < 6; i++) output[2 + i] = (uint8_t)((bits >> (8 * i)) & 0xFF);
}

static int encode_image(SDL_Surface *surface, int format, const char *output_path)
{
	int width  = surface->w;
	int height = surface->h;
	int block_size = format == 1 ? 8 : 16;
	int blocks_x = (width + 3) / 4;
	int blocks_y = (height + 3) / 4;
	size_t payload_size = (size_t)blocks_x * (size_t)blocks_y * (size_t)block_size;
	uint8_t *payload	= calloc(1, payload_size);
	if (payload == NULL) return -1;

	SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
	if (converted == NULL) {
		free(payload);
		return -1;
	}

	for (int by = 0; by < blocks_y; by++) {
		for (int bx = 0; bx < blocks_x; bx++) {
			uint8_t pixels[16][4];
			for (int py = 0; py < 4; py++) {
				for (int px = 0; px < 4; px++) {
					int x = bx * 4 + px;
					int y = by * 4 + py;
					if (x >= width) x = width - 1;
					if (y >= height) y = height - 1;
					const uint8_t *source =
						(const uint8_t *)converted->pixels + (size_t)y * converted->pitch + x * 4;
					memcpy(pixels[py * 4 + px], source, 4);
				}
			}
			uint8_t *block = payload + ((size_t)by * blocks_x + bx) * block_size;
			if (format == 1) {
				encode_color_block(pixels, block);
			} else {
				encode_alpha_block(pixels, block);
				encode_color_block(pixels, block + 8);
			}
		}
	}
	SDL_DestroySurface(converted);

	FILE *file = fopen(output_path, "wb");
	if (file == NULL) {
		free(payload);
		return -1;
	}
	uint8_t header[BCTX_HEADER_SIZE];
	memcpy(header, "BCTX", 4);
	header[4] = 1;
	header[5] = (uint8_t)format;
	header[6] = 0;
	header[7] = 0;
	uint32_t values[3] = {(uint32_t)width, (uint32_t)height, (uint32_t)payload_size};
	for (int i = 0; i < 3; i++) {
		header[8 + i * 4]	 = (uint8_t)(values[i] & 0xFF);
		header[9 + i * 4]	 = (uint8_t)((values[i] >> 8) & 0xFF);
		header[10 + i * 4] = (uint8_t)((values[i] >> 16) & 0xFF);
		header[11 + i * 4] = (uint8_t)((values[i] >> 24) & 0xFF);
	}
	int ok = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
			 fwrite(payload, 1, payload_size, file) == payload_size;
	fclose(file);
	free(payload);
	return ok ? 0 : -1;
}

int main(int argc, char **argv)
{
	const char *format_text = NULL;
	const char *output		= NULL;
	const char *input		= NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
			format_text = argv[++i];
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			output = argv[++i];
		} else if (argv[i][0] != '-') {
			input = argv[i];
		}
	}
	if (format_text == NULL || output == NULL || input == NULL) {
		fprintf(stderr, "usage: %s -f bc1|bc3 -o OUT.bctex IMAGE\n", argv[0]);
		return 1;
	}
	int format = strcmp(format_text, "bc1") == 0 ? 1 : (strcmp(format_text, "bc3") == 0 ? 3 : 0);
	if (format == 0) {
		fprintf(stderr, "-f must be bc1 or bc3\n");
		return 1;
	}

	SDL_SetAppMetadata("BridgeEngine bctex packer", "1.0", "com.bridgeengine.bctex");
	if (!SDL_Init(0)) {
		fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Surface *surface = IMG_Load(input);
	if (surface == NULL) {
		fprintf(stderr, "cannot load %s: %s\n", input, SDL_GetError());
		SDL_Quit();
		return 1;
	}
	int result = encode_image(surface, format, output);
	SDL_DestroySurface(surface);
	SDL_Quit();
	if (result != 0) {
		fprintf(stderr, "failed to write %s\n", output);
		return 1;
	}
	printf("bctex: %s (%s)\n", output, format_text);
	return 0;
}
