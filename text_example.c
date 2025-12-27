#include "BridgeEngine.h"
#include "bapi_internal.h"
#include <stdio.h>
#include <stdbool.h>

int main(int argc, char* argv[]) {
    if (bapi_engine_init("Text Example", 800, 600) != 0) {
        printf("Failed to initialize engine\n");
        return 1;
    }
    
    bapi_engine_render_create();
    
    if (bapi_text_init("ttf_example/example.ttf", 32) != 0) {
        printf("Failed to initialize text system\n");
        return 1;
    }
    
    bapi_texture_t redText = bapi_render_text("Red Text", bapi_color(255, 0, 0, 255));
    bapi_texture_t greenText = bapi_render_text("Green Text", bapi_color(0, 255, 0, 255));
    bapi_texture_t blueText = bapi_render_text("Blue Text", bapi_color(0, 0, 255, 255));
    bapi_texture_t whiteText = bapi_render_text("White Text", bapi_color(255, 255, 255, 255));
    bapi_texture_t chineseText = bapi_render_text("中文显示测试", bapi_color(255, 255, 0, 255));
    
    bool running = true;
    bapi_event_t event;
    
    while (running) {
        while (bapi_poll_event(&event)) {
            int type = bapi_event_get_type(&event);
            if (type == BAPI_EVENT_QUIT) {
                running = false;
            }
        }
        
        bapi_render_clear();
        
        if (redText) bapi_draw_text(redText, 50, 50, 200, 40);
        if (greenText) bapi_draw_text(greenText, 50, 100, 200, 40);
        if (blueText) bapi_draw_text(blueText, 50, 150, 200, 40);
        if (whiteText) bapi_draw_text(whiteText, 50, 200, 200, 40);
        if (chineseText) bapi_draw_text(chineseText, 50, 250, 300, 40);
        
        bapi_render_present();
        bapi_delay(16); 
    }
    

    if (redText) bapi_destroy_text(redText);
    if (greenText) bapi_destroy_text(greenText);
    if (blueText) bapi_destroy_text(blueText);
    if (whiteText) bapi_destroy_text(whiteText);
    if (chineseText) bapi_destroy_text(chineseText);
    
    bapi_text_cleanup();
    bapi_engine_quit();
    
    return 0;
}
