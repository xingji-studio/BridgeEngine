#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bapi_scene_internal* bapi_scene_t;

typedef void (*bapi_scene_on_enter_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_exit_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_update_fn)(bapi_scene_t scene, float delta_time);
typedef void (*bapi_scene_on_render_fn)(bapi_scene_t scene);

typedef struct {
    bapi_scene_on_enter_fn on_enter;
    bapi_scene_on_exit_fn on_exit;
    bapi_scene_on_update_fn on_update;
    bapi_scene_on_render_fn on_render;
    void* user_data;
} bapi_scene_callbacks_t;

typedef struct bapi_scene_manager_internal* bapi_scene_manager_t;

bapi_scene_t bapi_scene_create(const char* name, bapi_scene_callbacks_t callbacks);
void bapi_scene_destroy(bapi_scene_t scene);
const char* bapi_scene_get_name(bapi_scene_t scene);
void* bapi_scene_get_user_data(bapi_scene_t scene);
void bapi_scene_set_user_data(bapi_scene_t scene, void* user_data);

bapi_scene_manager_t bapi_scene_manager_create(void);
void bapi_scene_manager_destroy(bapi_scene_manager_t manager);
int bapi_scene_manager_add_scene(bapi_scene_manager_t manager, bapi_scene_t scene);
int bapi_scene_manager_switch_scene(bapi_scene_manager_t manager, const char* name);
bapi_scene_t bapi_scene_manager_get_current_scene(bapi_scene_manager_t manager);
bapi_scene_t bapi_scene_manager_get_scene(bapi_scene_manager_t manager, const char* name);
void bapi_scene_manager_update(bapi_scene_manager_t manager, float delta_time);
void bapi_scene_manager_render(bapi_scene_manager_t manager);

#ifdef __cplusplus
}
#endif
