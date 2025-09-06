#include "master/init.h"
#include "render/create.h"
#include "render/draw.h"
#include "mouse_drawing.h"
#include "text/text.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

extern SDL_Window *window;
extern SDL_Renderer *renderer;

int main(int argc, char* argv[]) {
    if (bapi_engine_init("BridgeEngine Demo", 800, 600) != 0) {
        SDL_Log("Failed to initialize engine\n");
        return 1;
    }
    
    bapi_engine_render_create();
    bapi_mouse_drawing_init();
    
    if (bapi_text_init("ttf_example/example.ttf", 24) != 0) {
        SDL_Log("Failed to initialize text system\n");
    }

    SDL_Texture* exampleImage = bapi_engine_render_load_image("image_example/XINGJI.png");
    if (exampleImage == NULL) {
        SDL_Log("Failed to load example image\n");
    }
    
    SDL_Texture* helloText = bapi_text_render("Hello BridgeEngine!", 0xffffffff);
    SDL_Texture* chineseText = bapi_text_render("你好，BridgeEngine！", 0x00ff00ff);
    SDL_Texture* studioText = bapi_text_render("XINGJI Studio", 0x00ff00ff);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { 
                running = false;
            } else {
                bapi_mouse_drawing_handle_event(&event);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        bapi_engine_render_fillrect(10, 10, 100, 100, 0x114514ff);
        bapi_engine_render_drawpixel(200, 200, 0xffffffff);
        bapi_engine_render_draw_triangle(300, 300, 400, 300, 200, 400, 0xffffffff);
        
        if (exampleImage != NULL) {
            bapi_engine_render_draw_image(exampleImage, 500, 100, 200, 200);
        }
        
        // 绘制文字
        if (helloText != NULL) {
            bapi_text_draw(helloText, 50, 50, 300, 30);
        }
        if (chineseText != NULL) {
            bapi_text_draw(chineseText, 50, 100, 300, 30);
        }
        if (studioText != NULL) {
            bapi_text_draw(studioText, 50, 150, 300, 30);
        }
        
        bapi_mouse_drawing_render();
        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    if (exampleImage != NULL) {
        bapi_engine_render_destroy_image(exampleImage);
    }
    if (helloText != NULL) {
        bapi_text_destroy(helloText);
    }
    if (chineseText != NULL) {
        bapi_text_destroy(chineseText);
    }
    if (studioText != NULL) {
        bapi_text_destroy(studioText);
    }
    
    bapi_mouse_drawing_cleanup();
    bapi_text_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}