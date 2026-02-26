#pragma once

#include "scene/scene.h"
#include "level/level.h"
#include <stdint.h>

#define MAX_SCENES 64
#define MAX_LEVELS 64
#define MAX_NAME_LENGTH 128
#define LEVEL_INDEX_MAP_SIZE 256

struct bapi_scene_internal {
    char name[MAX_NAME_LENGTH];
    uint32_t name_hash;
    bapi_scene_callbacks_t callbacks;
    void* user_data;
    int is_active;
};

struct bapi_scene_manager_internal {
    bapi_scene_t scenes[MAX_SCENES];
    int scene_count;
    bapi_scene_t current_scene;
};

struct bapi_level_internal {
    char name[MAX_NAME_LENGTH];
    uint32_t name_hash;
    int index;
    bapi_level_callbacks_t callbacks;
    void* user_data;
    int is_loaded;
};

struct bapi_level_manager_internal {
    bapi_level_t levels[MAX_LEVELS];
    bapi_level_t index_map[LEVEL_INDEX_MAP_SIZE];
    int level_count;
    bapi_level_t current_level;
    int current_index;
};

static inline uint32_t bapi_hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}
