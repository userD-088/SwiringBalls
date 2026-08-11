#include<iostream>
#include"MainHeader.h"
#include<vector>
#include<cmath>
#include<SDL3/SDL.h>

struct Ball {
	float x, y;
	float vx, vy;
	float radius;
	SDL_Color color;
};

void DrawBall(SDL_Renderer* renderer, const Ball& ball) {
    SDL_SetRenderDrawColor(renderer, ball.color.r, ball.color.g, ball.color.b, ball.color.a);

    for (float dy = -ball.radius; dy <= ball.radius; dy += 1.0f) {
        float dx = std::sqrt(ball.radius * ball.radius - dy * dy);

        SDL_RenderLine(
            renderer,
            ball.x - dx, ball.y + dy,
            ball.x + dx, ball.y + dy
        );
    }
}

void CollisionDetection(Ball& ball1, Ball& ball2) {
	float dx = ball2.x - ball1.x;
	float dy = ball2.y - ball1.y;
	float distance = std::sqrt(dx * dx + dy * dy);
	float minDistance = ball1.radius + ball2.radius;
	if (distance < minDistance) {
		float angle = std::atan2(dy, dx);
		float targetX = ball1.x + std::cos(angle) * minDistance;
		float targetY = ball1.y + std::sin(angle) * minDistance;
		float ax = (targetX - ball2.x) * 0.5f;
		float ay = (targetY - ball2.y) * 0.5f;
		ball1.vx -= ax;
		ball1.vy -= ay;
		ball2.vx += ax;
		ball2.vy += ay;
	}
}

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

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

	// Create balls
	std::vector <Ball> balls = { 
        { 200.0f, 300.0f, 2.0f, 1.5f, 20.0f, {255, 0, 0, 255} }, 
		{ 400.0f, 500.0f, -1.0f, 2.5f, 20.0f, {0, 255, 0, 255} },
		{ 600.0f, 200.0f, 1.5f, -2.0f, 20.0f, {0, 0, 255, 255} },
		{ 100.0f, 250.0f, 2.5f, 0.5f, 20.0f, {255, 255, 0, 255} },
		{ 300.0f, 100.0f, -1.5f, 1.5f, 20.0f, {255, 0, 255, 255} },
		{ 700.0f, 550.0f, 1.0f, -0.5f, 20.0f, {0, 255, 255, 255} },
		{ 600.0f, 150.0f, -0.5f, 2.5f, 20.0f, {255, 255, 255, 255} }
    };

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}
		
        SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, 1.0f);
		SDL_RenderClear(renderer);

        for (auto& ball : balls) {
            ball.x += ball.vx;
            ball.y += ball.vy;

            if (ball.x - ball.radius < 0 || ball.x + ball.radius > SCREEN_WIDTH) ball.vx *= -1;
            if (ball.y - ball.radius < 0 || ball.y + ball.radius > SCREEN_HEIGHT) ball.vy *= -1;
        }

		// Collision detection
		for (size_t i = 0; i < balls.size(); ++i) {
			for (size_t j = i + 1; j < balls.size(); ++j) {
				CollisionDetection(balls[i], balls[j]);
			}
		}

		// Rendering code goes here

		for (const auto& ball : balls) {
			DrawBall(renderer, ball);
		}

		SDL_RenderPresent(renderer);

		SDL_Delay(8); //120 FPS
	}

	SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

	return 0;
}
