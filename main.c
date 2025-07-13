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

static void arg_parser(int count, char *arg[])
{
    if (count > 1) {
        if (strcmp(arg[1], "--help") == 0 || strcmp(arg[1], "-h") == 0) {
            printf("BridgeEngine demo\n");
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char *argv[])
{
    arg_parser(argc, argv);

    if (engine_init("demo", 800, 600) != 0) {
        SDL_Log("Exit...\n");
        SDL_Quit();
        return 1;
    }

    engine_render_create();
    mouse_drawing_init();

    engine_render_fillrect(10, 10, 100, 100, 0x114514ff);
    engine_render_drawpixel(200, 200, 0xffffffff);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { 
                running = false;
            } else {
                mouse_drawing_handle_event(&event);
            }
        }

        mouse_drawing_render();
        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    mouse_drawing_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
