#pragma once

// BridgeEngine头文件

#include "master/init.h"
#include "render/create.h"
#include "render/draw.h"
#include "mouse_drawing.h"
#include "text/text.h"

#define BRIDGEENGINE_VERSION "1.0.0"
#define BRIDGEENGINE_MAJOR 1
#define BRIDGEENGINE_MINOR 0
#define BRIDGEENGINE_PATCH 0

#ifdef __cplusplus
extern "C" {
#endif

const char* bridgeengine_get_version(void);
int bridgeengine_get_version_number(void);

#ifdef __cplusplus
}
#endif