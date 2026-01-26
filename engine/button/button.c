#include "button/button.h"
#include "log/log.h"
#include "render/draw.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bapi_button_t* bapi_create_button(float x, float y, float w, float h, const char* text, bapi_color_t normal_color, bapi_color_t hover_color, bapi_color_t click_color, bapi_color_t text_color, float text_size) {
    bapi_button_t* button = malloc(sizeof(bapi_button_t));
    if (button == NULL) {
        return NULL;
    }
    
    button->rect.x = x;
    button->rect.y = y;
    button->rect.w = w;
    button->rect.h = h;
    button->normal_color = normal_color;
    button->hover_color = hover_color;
    button->click_color = click_color;
    button->text = text;
    button->text_color = text_color;
    button->text_size = text_size;
    button->is_clicked = 0;
    button->is_hovered = 0;
    
    button->text_width = 0;
    button->text_height = 0;
    
    return button;
}

void bapi_destroy_button(bapi_button_t* button) {
    if (button != NULL) {
        free(button);
    }
}

int bapi_button_update(bapi_button_t* button, const bapi_event_t* event) {
    if (button == NULL || event == NULL) {
        return 0;
    }
    
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    int inside = (mouse_x >= button->rect.x && mouse_x <= button->rect.x + button->rect.w &&
                  mouse_y >= button->rect.y && mouse_y <= button->rect.y + button->rect.h);
    
    button->is_hovered = inside;

    if (bapi_event_is_mouse_button_down(event) && inside) {
        button->is_clicked = 1;
    } else if (bapi_event_is_mouse_button_up(event)) {
        int was_clicked = button->is_clicked;
        button->is_clicked = 0;
        return was_clicked && inside;
    }
    
    return 0;
}

void bapi_button_render(bapi_button_t* button) {
    if (button == NULL) {
        return;
    }
    
    bapi_color_t current_color;
    
    if (button->is_clicked) {
        current_color = button->click_color;
    } else if (button->is_hovered) {
        current_color = button->hover_color;
    } else {
        current_color = button->normal_color;
    }

    bapi_fill_rect(button->rect.x, button->rect.y, button->rect.w, button->rect.h, current_color);

    bapi_draw_rect(button->rect.x, button->rect.y, button->rect.w, button->rect.h, bapi_color(0, 0, 0, 255));

    if (button->text != NULL) {
        float estimated_text_width = strlen(button->text) * button->text_size * 0.5f;
        float estimated_text_height = button->text_size * 1.50f;
        float text_x = button->rect.x + (button->rect.w - estimated_text_width) / 2;
        float text_y = button->rect.y + (button->rect.h - estimated_text_height) / 2;
        
        bapi_draw_text(button->text, text_x, text_y, button->text_size, button->text_color);
    }
}

int bapi_button_is_clicked(bapi_button_t* button) {
    if (button == NULL) {
        return 0;
    }
    return button->is_clicked;
}

int bapi_button_is_hovered(bapi_button_t* button) {
    if (button == NULL) {
        return 0;
    }
    return button->is_hovered;
}