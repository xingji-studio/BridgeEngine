#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_level_internal* bapi_level_t;

typedef void (*bapi_level_on_load_fn)(bapi_level_t level);
typedef void (*bapi_level_on_unload_fn)(bapi_level_t level);
typedef void (*bapi_level_on_update_fn)(bapi_level_t level, float delta_time);
typedef void (*bapi_level_on_render_fn)(bapi_level_t level);

typedef struct {
    bapi_level_on_load_fn on_load;
    bapi_level_on_unload_fn on_unload;
    bapi_level_on_update_fn on_update;
    bapi_level_on_render_fn on_render;
    void* user_data;
} bapi_level_callbacks_t;

typedef struct bapi_level_manager_internal* bapi_level_manager_t;

bapi_level_t bapi_level_create(const char* name, int index, bapi_level_callbacks_t callbacks);
void bapi_level_destroy(bapi_level_t level);
const char* bapi_level_get_name(bapi_level_t level);
int bapi_level_get_index(bapi_level_t level);
void* bapi_level_get_user_data(bapi_level_t level);
void bapi_level_set_user_data(bapi_level_t level, void* user_data);

bapi_level_manager_t bapi_level_manager_create(void);
void bapi_level_manager_destroy(bapi_level_manager_t manager);
int bapi_level_manager_add_level(bapi_level_manager_t manager, bapi_level_t level);
int bapi_level_manager_load_level(bapi_level_manager_t manager, const char* name);
int bapi_level_manager_load_level_by_index(bapi_level_manager_t manager, int index);
int bapi_level_manager_next_level(bapi_level_manager_t manager);
int bapi_level_manager_previous_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_current_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_level(bapi_level_manager_t manager, const char* name);
bapi_level_t bapi_level_manager_get_level_by_index(bapi_level_manager_t manager, int index);
int bapi_level_manager_get_level_count(bapi_level_manager_t manager);
void bapi_level_manager_update(bapi_level_manager_t manager, float delta_time);
void bapi_level_manager_render(bapi_level_manager_t manager);

#ifdef __cplusplus
}
#endif
