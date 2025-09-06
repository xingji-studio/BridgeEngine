#include "master/init.h"
#include "render/create.h"
#include "text/text.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdbool.h>
// 文字显示单独示例
extern SDL_Window *window;
extern SDL_Renderer *renderer;

int main(int argc, char* argv[]) {
    if (bapi_engine_init("Text Example", 800, 600) != 0) {
        SDL_Log("Failed to initialize engine\n");
        return 1;
    }
    
    bapi_engine_render_create();
    
    if (bapi_text_init("ttf_example/example.ttf", 32) != 0) {
        SDL_Log("Failed to initialize text system\n");
        return 1;
    }
    
    SDL_Texture* redText = bapi_text_render("Red Text", 0xff0000ff);
    SDL_Texture* greenText = bapi_text_render("Green Text", 0x00ff00ff);
    SDL_Texture* blueText = bapi_text_render("Blue Text", 0x0000ffff);
    SDL_Texture* whiteText = bapi_text_render("White Text", 0xffffffff);
    SDL_Texture* chineseText = bapi_text_render("中文显示测试", 0xffff00ff);
    
    bool running = true;
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        if (redText) bapi_text_draw(redText, 50, 50, 200, 40);
        if (greenText) bapi_text_draw(greenText, 50, 100, 200, 40);
        if (blueText) bapi_text_draw(blueText, 50, 150, 200, 40);
        if (whiteText) bapi_text_draw(whiteText, 50, 200, 200, 40);
        if (chineseText) bapi_text_draw(chineseText, 50, 250, 300, 40);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16); 
    }
    

    if (redText) bapi_text_destroy(redText);
    if (greenText) bapi_text_destroy(greenText);
    if (blueText) bapi_text_destroy(blueText);
    if (whiteText) bapi_text_destroy(whiteText);
    if (chineseText) bapi_text_destroy(chineseText);
    
    bapi_text_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}