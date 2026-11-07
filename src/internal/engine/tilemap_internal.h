#pragma once

#include "BridgeEngine.h"

struct bapi_tilemap_internal {
	int			   width;
	int			   height;
	int			   tile_width;
	int			   tile_height;
	int			  *tiles;
	uint8_t		  *solid;
	bapi_texture_t tileset;
	int			   columns;
};
