#include "master/master.h"
#include "publics.h"
#include "render/render.h"
#include "mouse_drawing.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern SDL_Window *window;
extern SDL_Renderer *renderer;

static void arg_parser(int count, char *arg[]);

int main(int argc, char *argv[])
{
    arg_parser(argc, argv);

    if (bapi_engine_init("demo", 800, 600) != 0) {
        SDL_Log("Exit...\n");
        SDL_Quit();
        return 1;
    }

    bapi_engine_render_create();
    bapi_mouse_drawing_init();

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
        
        bapi_mouse_drawing_render();
        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    bapi_mouse_drawing_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

static void arg_parser(int count, char *arg[])
{
    if (count > 1) {
        if (strcmp(arg[1], "--help") == 0 || strcmp(arg[1], "-h") == 0) {
            printf("BridgeEngine demo\n");
            exit(EXIT_SUCCESS);
        }
    }
}