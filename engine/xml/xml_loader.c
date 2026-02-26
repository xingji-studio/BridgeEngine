#include "xml/xml_loader.h"
#include "scene/scene_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


//Hand make xml loader, not use libxml2:( XJ380 Not support libxml2

#define MAX_LINE_LENGTH 512
#define MAX_ATTR_LENGTH 128

static const char* find_char(const char* str, char c) {
    while (*str && *str != c) str++;
    return *str == c ? str : NULL;
}

static const char* skip_whitespace(const char* str) {
    while (*str && isspace((unsigned char)*str)) str++;
    return str;
}

static int parse_attribute_inline(const char* attr_start, const char* attr_end, 
                                   char* name_out, char* value_out) {
    const char* eq = find_char(attr_start, '=');
    if (!eq || eq >= attr_end) return -1;
    
    int name_len = (int)(eq - attr_start);
    if (name_len >= MAX_ATTR_LENGTH) name_len = MAX_ATTR_LENGTH - 1;
    memcpy(name_out, attr_start, name_len);
    name_out[name_len] = '\0';
    
    const char* quote1 = find_char(eq, '"');
    if (!quote1 || quote1 >= attr_end) return -1;
    
    const char* quote2 = find_char(quote1 + 1, '"');
    if (!quote2 || quote2 > attr_end) return -1;
    
    int value_len = (int)(quote2 - quote1 - 1);
    if (value_len >= MAX_ATTR_LENGTH) value_len = MAX_ATTR_LENGTH - 1;
    memcpy(value_out, quote1 + 1, value_len);
    value_out[value_len] = '\0';
    
    return 0;
}

static const char* get_attr_value_inline(const char* line_start, const char* line_end,
                                          const char* attr_name, char* value_out) {
    const char* pos = line_start;
    size_t attr_name_len = strlen(attr_name);
    
    while (pos < line_end) {
        const char* eq = find_char(pos, '=');
        if (!eq || eq >= line_end) break;
        
        const char* name_start = pos;
        while (name_start < eq && isspace((unsigned char)*name_start)) name_start++;
        
        int name_len = (int)(eq - name_start);
        while (name_len > 0 && isspace((unsigned char)name_start[name_len - 1])) name_len--;
        
        if ((size_t)name_len == attr_name_len && memcmp(name_start, attr_name, attr_name_len) == 0) {
            const char* quote1 = find_char(eq, '"');
            if (!quote1 || quote1 >= line_end) break;
            
            const char* quote2 = find_char(quote1 + 1, '"');
            if (!quote2 || quote2 > line_end) break;
            
            int value_len = (int)(quote2 - quote1 - 1);
            if (value_len >= MAX_ATTR_LENGTH) value_len = MAX_ATTR_LENGTH - 1;
            memcpy(value_out, quote1 + 1, value_len);
            value_out[value_len] = '\0';
            return value_out;
        }
        
        const char* quote1 = find_char(eq, '"');
        if (!quote1 || quote1 >= line_end) break;
        
        const char* quote2 = find_char(quote1 + 1, '"');
        if (!quote2 || quote2 > line_end) break;
        
        pos = quote2 + 1;
    }
    
    return NULL;
}

static int get_tag_name(const char* line, char* tag_out) {
    const char* start = find_char(line, '<');
    if (!start) return -1;
    
    start++;
    start = skip_whitespace(start);
    
    const char* end = find_char(start, '>');
    if (!end) return -1;
    
    const char* space = find_char(start, ' ');
    const char* tag_end = space && space < end ? space : end;
    
    int len = (int)(tag_end - start);
    if (len >= MAX_ATTR_LENGTH) len = MAX_ATTR_LENGTH - 1;
    memcpy(tag_out, start, len);
    tag_out[len] = '\0';
    
    return 0;
}

bapi_scene_manager_t bapi_scene_manager_load_from_xml(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return NULL;
    
    bapi_scene_manager_t manager = bapi_scene_manager_create();
    if (!manager) {
        fclose(file);
        return NULL;
    }
    
    char line[MAX_LINE_LENGTH];
    char scene_name[MAX_ATTR_LENGTH] = {0};
    bapi_scene_callbacks_t callbacks = {0};
    
    while (fgets(line, sizeof(line), file)) {
        char tag_name[MAX_ATTR_LENGTH];
        if (get_tag_name(line, tag_name) != 0) continue;
        
        if (strcmp(tag_name, "scene") == 0) {
            const char* line_end = find_char(line, '>');
            if (line_end) {
                char attr_value[MAX_ATTR_LENGTH];
                if (get_attr_value_inline(line, line_end, "name", attr_value)) {
                    strncpy(scene_name, attr_value, MAX_ATTR_LENGTH - 1);
                    scene_name[MAX_ATTR_LENGTH - 1] = '\0';
                }
            }
            memset(&callbacks, 0, sizeof(callbacks));
        } else if (strcmp(tag_name, "/scene") == 0) {
            if (scene_name[0] != '\0') {
                bapi_scene_t scene = bapi_scene_create(scene_name, callbacks);
                if (scene) {
                    bapi_scene_manager_add_scene(manager, scene);
                }
            }
            scene_name[0] = '\0';
        }
    }
    
    fclose(file);
    return manager;
}

bapi_level_manager_t bapi_level_manager_load_from_xml(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return NULL;
    
    bapi_level_manager_t manager = bapi_level_manager_create();
    if (!manager) {
        fclose(file);
        return NULL;
    }
    
    char line[MAX_LINE_LENGTH];
    char level_name[MAX_ATTR_LENGTH] = {0};
    int level_index = 0;
    bapi_level_callbacks_t callbacks = {0};
    
    while (fgets(line, sizeof(line), file)) {
        char tag_name[MAX_ATTR_LENGTH];
        if (get_tag_name(line, tag_name) != 0) continue;
        
        if (strcmp(tag_name, "level") == 0) {
            const char* line_end = find_char(line, '>');
            if (line_end) {
                char attr_value[MAX_ATTR_LENGTH];
                if (get_attr_value_inline(line, line_end, "name", attr_value)) {
                    strncpy(level_name, attr_value, MAX_ATTR_LENGTH - 1);
                    level_name[MAX_ATTR_LENGTH - 1] = '\0';
                }
                if (get_attr_value_inline(line, line_end, "index", attr_value)) {
                    level_index = atoi(attr_value);
                }
            }
            memset(&callbacks, 0, sizeof(callbacks));
        } else if (strcmp(tag_name, "/level") == 0) {
            if (level_name[0] != '\0') {
                bapi_level_t level = bapi_level_create(level_name, level_index, callbacks);
                if (level) {
                    bapi_level_manager_add_level(manager, level);
                }
            }
            level_name[0] = '\0';
        }
    }
    
    fclose(file);
    return manager;
}

int bapi_scene_manager_save_to_xml(bapi_scene_manager_t manager, const char* filepath) {
    if (!manager || !filepath) return -1;
    
    FILE* file = fopen(filepath, "w");
    if (!file) return -1;
    
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<scenes>\n", file);
    
    for (int i = 0; i < manager->scene_count; i++) {
        bapi_scene_t scene = manager->scenes[i];
        if (scene) {
            fprintf(file, "  <scene name=\"%s\" />\n", scene->name);
        }
    }
    
    fputs("</scenes>\n", file);
    fclose(file);
    return 0;
}

int bapi_level_manager_save_to_xml(bapi_level_manager_t manager, const char* filepath) {
    if (!manager || !filepath) return -1;
    
    FILE* file = fopen(filepath, "w");
    if (!file) return -1;
    
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<levels>\n", file);
    
    for (int i = 0; i < manager->level_count; i++) {
        bapi_level_t level = manager->levels[i];
        if (level) {
            fprintf(file, "  <level name=\"%s\" index=\"%d\" />\n", level->name, level->index);
        }
    }
    
    fputs("</levels>\n", file);
    fclose(file);
    return 0;
}
