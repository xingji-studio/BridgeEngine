#include "internal/engine/texture_bc.h"

#include <string.h>

static void bc565_to_rgba(uint16_t color, uint8_t *rgba)
{
	uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * 255 / 31);
	uint8_t g = (uint8_t)(((color >> 5) & 0x3F) * 255 / 63);
	uint8_t b = (uint8_t)((color & 0x1F) * 255 / 31);
	rgba[0]   = r;
	rgba[1]   = g;
	rgba[2]   = b;
	rgba[3]   = 255;
}

static void bc1_block(const uint8_t *block, uint8_t *rgba, int width, int height, int x, int y)
{
	uint16_t c0 = (uint16_t)(block[0] | (block[1] << 8));
	uint16_t c1 = (uint16_t)(block[2] | (block[3] << 8));
	uint8_t  colors[4][4];
	bc565_to_rgba(c0, colors[0]);
	bc565_to_rgba(c1, colors[1]);
	if (c0 > c1) {
		for (int i = 0; i < 3; i++) {
			colors[2][i] = (uint8_t)((2 * colors[0][i] + colors[1][i]) / 3);
			colors[3][i] = (uint8_t)((colors[0][i] + 2 * colors[1][i]) / 3);
		}
		colors[2][3] = colors[3][3] = 255;
	} else {
		for (int i = 0; i < 3; i++) {
			colors[2][i] = (uint8_t)((colors[0][i] + colors[1][i]) / 2);
		}
		colors[2][3] = 255;
		colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
	}

	uint32_t indices = (uint32_t)block[4] | ((uint32_t)block[5] << 8) | ((uint32_t)block[6] << 16) |
					   ((uint32_t)block[7] << 24);
	for (int py = 0; py < 4; py++) {
		for (int px = 0; px < 4; px++) {
			int tx = x + px;
			int ty = y + py;
			if (tx >= width || ty >= height) continue;
			int index = (indices >> (2 * (py * 4 + px))) & 3;
			memcpy(rgba + ((size_t)ty * width + tx) * 4, colors[index], 4);
		}
	}
}

static void bc3_alpha_block(const uint8_t *block, uint8_t alpha[16])
{
	uint8_t a0 = block[0];
	uint8_t a1 = block[1];
	uint8_t values[8];
	values[0] = a0;
	values[1] = a1;
	if (a0 > a1) {
		for (int i = 1; i <= 6; i++) {
			values[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
		}
	} else {
		for (int i = 1; i <= 4; i++) {
			values[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
		}
		values[6] = 0;
		values[7] = 255;
	}

	uint64_t bits = 0;
	for (int i = 0; i < 6; i++) bits |= (uint64_t)block[2 + i] << (8 * i);
	for (int i = 0; i < 16; i++) alpha[i] = values[(bits >> (3 * i)) & 7];
}

int bapi_bc_decode(const void *data, size_t size, int format, int width, int height, uint8_t *rgba)
{
	if (data == NULL || rgba == NULL || width <= 0 || height <= 0) return -1;
	if (format != BAPI_BC_FORMAT_BC1 && format != BAPI_BC_FORMAT_BC3) return -1;

	int block_size = format == BAPI_BC_FORMAT_BC1 ? 8 : 16;
	int blocks_x   = (width + 3) / 4;
	int blocks_y   = (height + 3) / 4;
	size_t needed  = (size_t)blocks_x * (size_t)blocks_y * (size_t)block_size;
	if (size < needed) return -1;

	const uint8_t *cursor = (const uint8_t *)data;
	for (int by = 0; by < blocks_y; by++) {
		for (int bx = 0; bx < blocks_x; bx++) {
			if (format == BAPI_BC_FORMAT_BC1) {
				bc1_block(cursor, rgba, width, height, bx * 4, by * 4);
			} else {
				uint8_t alpha[16];
				bc3_alpha_block(cursor, alpha);
				uint8_t block_rgba[16][4];
				bc1_block(cursor + 8, (uint8_t *)block_rgba, 4, 4, 0, 0);
				for (int py = 0; py < 4; py++) {
					for (int px = 0; px < 4; px++) {
						int tx = bx * 4 + px;
						int ty = by * 4 + py;
						if (tx >= width || ty >= height) continue;
						memcpy(rgba + ((size_t)ty * width + tx) * 4, block_rgba[py * 4 + px], 4);
						rgba[((size_t)ty * width + tx) * 4 + 3] = alpha[py * 4 + px];
					}
				}
			}
			cursor += block_size;
		}
	}
	return 0;
}
