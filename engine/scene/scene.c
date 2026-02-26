#include "scene/scene.h"
#include "scene/scene_internal.h"
#include <stdlib.h>
#include <string.h>

bapi_scene_t bapi_scene_create(const char* name, bapi_scene_callbacks_t callbacks) {
    if (!name) return NULL;
    
    bapi_scene_t scene = (bapi_scene_t)malloc(sizeof(struct bapi_scene_internal));
    if (!scene) return NULL;
    
    strncpy(scene->name, name, MAX_NAME_LENGTH - 1);
    scene->name[MAX_NAME_LENGTH - 1] = '\0';
    scene->name_hash = bapi_hash_string(name);
    scene->callbacks = callbacks;
    scene->user_data = callbacks.user_data;
    scene->is_active = 0;
    
    return scene;
}

void bapi_scene_destroy(bapi_scene_t scene) {
    if (scene) {
        free(scene);
    }
}

const char* bapi_scene_get_name(bapi_scene_t scene) {
    return scene ? scene->name : NULL;
}

void* bapi_scene_get_user_data(bapi_scene_t scene) {
    return scene ? scene->user_data : NULL;
}

void bapi_scene_set_user_data(bapi_scene_t scene, void* user_data) {
    if (scene) {
        scene->user_data = user_data;
    }
}

bapi_scene_manager_t bapi_scene_manager_create(void) {
    bapi_scene_manager_t manager = (bapi_scene_manager_t)malloc(sizeof(struct bapi_scene_manager_internal));
    if (!manager) return NULL;
    
    memset(manager->scenes, 0, sizeof(manager->scenes));
    manager->scene_count = 0;
    manager->current_scene = NULL;
    
    return manager;
}

void bapi_scene_manager_destroy(bapi_scene_manager_t manager) {
    if (manager) {
        for (int i = 0; i < manager->scene_count; i++) {
            bapi_scene_destroy(manager->scenes[i]);
        }
        free(manager);
    }
}

int bapi_scene_manager_add_scene(bapi_scene_manager_t manager, bapi_scene_t scene) {
    if (!manager || !scene) return -1;
    if (manager->scene_count >= MAX_SCENES) return -2;
    
    manager->scenes[manager->scene_count++] = scene;
    return 0;
}

int bapi_scene_manager_switch_scene(bapi_scene_manager_t manager, const char* name) {
    if (!manager || !name) return -1;
    
    uint32_t target_hash = bapi_hash_string(name);
    bapi_scene_t new_scene = NULL;
    
    for (int i = 0; i < manager->scene_count; i++) {
        if (manager->scenes[i]->name_hash == target_hash) {
            if (strcmp(manager->scenes[i]->name, name) == 0) {
                new_scene = manager->scenes[i];
                break;
            }
        }
    }
    
    if (!new_scene) return -2;
    
    if (manager->current_scene) {
        manager->current_scene->is_active = 0;
        if (manager->current_scene->callbacks.on_exit) {
            manager->current_scene->callbacks.on_exit(manager->current_scene);
        }
    }
    
    new_scene->is_active = 1;
    if (new_scene->callbacks.on_enter) {
        new_scene->callbacks.on_enter(new_scene);
    }
    
    manager->current_scene = new_scene;
    return 0;
}

bapi_scene_t bapi_scene_manager_get_current_scene(bapi_scene_manager_t manager) {
    return manager ? manager->current_scene : NULL;
}

bapi_scene_t bapi_scene_manager_get_scene(bapi_scene_manager_t manager, const char* name) {
    if (!manager || !name) return NULL;
    
    uint32_t target_hash = bapi_hash_string(name);
    
    for (int i = 0; i < manager->scene_count; i++) {
        if (manager->scenes[i]->name_hash == target_hash) {
            if (strcmp(manager->scenes[i]->name, name) == 0) {
                return manager->scenes[i];
            }
        }
    }
    
    return NULL;
}

void bapi_scene_manager_update(bapi_scene_manager_t manager, float delta_time) {
    if (manager && manager->current_scene && manager->current_scene->callbacks.on_update) {
        manager->current_scene->callbacks.on_update(manager->current_scene, delta_time);
    }
}

void bapi_scene_manager_render(bapi_scene_manager_t manager) {
    if (manager && manager->current_scene && manager->current_scene->callbacks.on_render) {
        manager->current_scene->callbacks.on_render(manager->current_scene);
    }
}
