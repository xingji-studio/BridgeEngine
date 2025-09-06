#include "BridgeEngine.h"

const char* bridgeengine_get_version(void) {
    return BRIDGEENGINE_VERSION;
}

int bridgeengine_get_version_number(void) {
    return BRIDGEENGINE_MAJOR * 10000 + BRIDGEENGINE_MINOR * 100 + BRIDGEENGINE_PATCH;
}