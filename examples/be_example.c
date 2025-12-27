#include <BridgeEngine.h>
#include "bapi_internal.h"
#include <stdio.h>
#include <stdbool.h>

int main() {
    printf("BridgeEngine Version: %s\n", bridgeengine_get_version());
    printf("Version Number: %d\n", bridgeengine_get_version_number());
    
    if (bapi_engine_init("BridgeEngine Example", 640, 480) != 0) {
        printf("Failed to initialize engine\n");
        return 1;
    }
    
    bapi_engine_render_create();

    if (bapi_text_init("example.ttf", 20) != 0) {
        printf("Failed to initialize text system\n");
    }
    
    printf("Engine start successfully!\n");
    printf("Press any key in the window to exit...\n");
    
    bool running = true;
    bapi_event_t event;
    
    while (running) {
        while (bapi_poll_event(&event)) {
            int type = bapi_event_get_type(&event);
            if (type == BAPI_EVENT_QUIT || type == BAPI_EVENT_KEY_DOWN) { 
                running = false;
            }
        }
        
        bapi_render_clear();
        
        bapi_fill_rect(10, 10, 100, 100, bapi_color_from_hex(0xff0000ff));
        bapi_draw_triangle(150, 50, 200, 150, 100, 150, bapi_color_from_hex(0x00ff00ff));
        
        bapi_texture_t XINGJIImage = bapi_load_image("XINGJI.png");
        if (XINGJIImage != NULL) {
            bapi_draw_image(XINGJIImage, 200, 200, 200, 200);
            bapi_destroy_texture(XINGJIImage);
        }
        
        bapi_texture_t text = bapi_render_text("Hello , This is BridegeEngine!", bapi_color(255, 255, 255, 255));
        bapi_texture_t text2 = bapi_render_text("By XINGJI Studio", bapi_color(255, 255, 255, 255));
        if (text != NULL) {
            bapi_draw_text(text, 10, 200, 200, 30);
            bapi_destroy_text(text);
        }
        if (text2 != NULL) {
            bapi_draw_text(text2, 10, 250, 200, 30);
            bapi_destroy_text(text2);
        }
        
        bapi_render_present();
        bapi_delay(16);
    }
    
    bapi_text_cleanup();
    bapi_engine_quit();
    
    return 0;
}
