#include<iostream>
#include"MainHeader.h"
#include<SDL3/SDL.h>

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        WINDOW_TITLE.c_str(),
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );

    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}
		// Rendering code goes here
	}

    SDL_DestroyWindow(window);
    SDL_Quit();

	return 0;
}
