#include <SDL3/SDL.h>
#include <stdio.h>

SDL_Window *window;

int bapi_engine_init(char *title, int width, int height)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow(title, width, height, 0);

	if (window == NULL) {
		SDL_Log("Failed to create window: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	return 0;
}