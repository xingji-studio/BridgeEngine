#include <BridgeEngine.h>
#include <stdio.h>

extern SDL_Window *window;
extern SDL_Renderer *renderer;

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
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN) { 
                running = false;
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderClear(renderer);
        
        bapi_engine_render_fillrect(10, 10, 100, 100, 0xff0000ff);
        bapi_engine_render_draw_triangle(150, 50, 200, 150, 100, 150, 0x00ff00ff);
        
        SDL_Texture* bapi_engine_render_load_image(const char* filepath);
        SDL_Texture* XINGJIImage = bapi_engine_render_load_image("XINGJI.png");
        bapi_engine_render_draw_image(XINGJIImage, 200, 200, 200, 200);
        SDL_Texture* text = bapi_text_render("Hello , This is BridegeEngine!", 0xffffffff);
        SDL_Texture* text2 = bapi_text_render("By XINGJI Studio", 0xffffffff);
        if (text != NULL) {
            bapi_text_draw(text, 10, 200, 200, 30);
            bapi_text_destroy(text);
        }
        if (text2 != NULL) {
            bapi_text_draw(text2, 10, 250, 200, 30);
            bapi_text_destroy(text2);
        }
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    bapi_text_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}