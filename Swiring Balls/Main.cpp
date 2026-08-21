#include <iostream>
#include "MainHeader.h"
#include <vector>
#include <cmath>
#include <SDL3/SDL.h>
#include <random>
#include <SDL3/SDL_ttf.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#ifdef _WIN32
static ULONGLONG FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

// Function to get the current process RAM usage in MB
size_t GetProcessRAMUsageMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
        return info.WorkingSetSize / (1024 * 1024);
    }
#endif
    return 0;
}

// CPUUsageTracker class to track CPU usage
class CPUUsageTracker {
#ifdef _WIN32
    ULONGLONG lastKernelTime = 0;
    ULONGLONG lastUserTime = 0;
    ULONGLONG lastSystemTime = 0;
    int numProcessors = 1;
#endif

public:
    CPUUsageTracker() {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
        Update();
#endif
    }

    int GetCoreCount() const {
#ifdef _WIN32
        return numProcessors;
#else
        return 1;
#endif
    }

    int GetCurrentCoreID() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessorNumber());
#else
        return 0;
#endif
    }

    // Parameters: showPerCore = true -> 0-100% per Core; showPerCore = false -> 0-100% Total System
    float Update(bool showPerCore = false) {
#ifdef _WIN32
        FILETIME ftime, fsys, fuser;
        FILETIME fcreation, fexit, fkernel, fuserProc;

        GetSystemTimeAsFileTime(&ftime);
        GetSystemStoreTime(&fsys, &fuser);
        GetProcessTimes(GetCurrentProcess(), &fcreation, &fexit, &fkernel, &fuserProc);

        ULONGLONG currentSystemTime = FileTimeToUint64(ftime);
        ULONGLONG currentKernelTime = FileTimeToUint64(fkernel);
        ULONGLONG currentUserTime = FileTimeToUint64(fuserProc);

        ULONGLONG sysDelta = currentSystemTime - lastSystemTime;
        ULONGLONG procDelta = (currentKernelTime - lastKernelTime) + (currentUserTime - lastUserTime);

        lastSystemTime = currentSystemTime;
        lastKernelTime = currentKernelTime;
        lastUserTime = currentUserTime;

        if (sysDelta > 0) {
            float usage = (static_cast<float>(procDelta) / sysDelta) * 100.0f;
            if (!showPerCore && numProcessors > 0) {
                usage /= numProcessors; // Normalize over all cores
            }
            return usage;
        }
#endif
        return 0.0f;
    }

private:
#ifdef _WIN32
    void GetSystemStoreTime(FILETIME* sys, FILETIME* user) {
        FILETIME idle;
        GetSystemTimes(&idle, sys, user);
    }
#endif
};

// Ball struct
struct Ball {
    float x, y;
    float vx, vy;
    float radius;
    SDL_Color color;
};

// Function to draw a filled circle (ball) using SDL_Renderer
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

// Function to handle collision detection and response between two balls
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

// Function to render text using SDL_ttf
void RenderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, float x, float y, SDL_Color color) {
    if (!font || text.empty()) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.length(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_FRect dstRect = { x, y, static_cast<float>(surface->w), static_cast<float>(surface->h) };
        SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

// Main function
int main(int argc, char* argv[])
{
	// Initialize SDL and TTF
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        SDL_Quit();
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

	// Create a renderer with hardware acceleration and vsync
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	// Load font
    TTF_Font* font = TTF_OpenFont("Extensions\\fonts\\Roboto-Regular.ttf", 24.0f);
    if (!font) {
        SDL_Log("Font could not be loaded: %s", SDL_GetError());
    }

	// Variable initialization
    bool running = true;
    SDL_Event event;

    size_t currentRamMB = 0;
    float currentCpuUsage = 0.0f;
    int currentCoreID = 0;
    bool showSingleCoreView = false; // Toggle with 'C' key

    CPUUsageTracker cpuTracker;

    std::random_device rd;
    std::uniform_real_distribution<float> distrib(0.0f, 1.0f);

	// Create balls
    std::vector<Ball> balls;
	int numBalls = 1000; // Adjust the number of balls as needed

    for (int i = 0; i < numBalls; ++i) {
        float radius = distrib(rd) * 3.0f + 1.0f;
        float x = distrib(rd) * (SCREEN_WIDTH - 2 * radius) + radius;
        float y = distrib(rd) * (SCREEN_HEIGHT - 2 * radius) + radius;
        float vx = distrib(rd) - 0.5f;
        float vy = distrib(rd) - 0.5f;
        SDL_Color color = { static_cast<Uint8>(distrib(rd) * 255), static_cast<Uint8>(distrib(rd) * 255), static_cast<Uint8>(distrib(rd) * 255), 255 };
        balls.push_back({ x, y, vx, vy, radius, color });
    }

	// FPS variables
    Uint64 lastTime = SDL_GetTicks();
    int frameCount = 0;
    int fps = 0;

	// Main loop
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                // Press 'C' to toggle CPU view (System-wide vs. Single Core)
                if (event.key.key == SDLK_C) {
                    showSingleCoreView = !showSingleCoreView;
                    currentCpuUsage = cpuTracker.Update(showSingleCoreView);
                }
            }
        }

		// Update ball positions and handle collisions
        for (auto& ball : balls) {
            ball.x += ball.vx;
            ball.y += ball.vy;

            if (ball.x - ball.radius < 0) { ball.x = ball.radius; ball.vx *= -1; }
            else if (ball.x + ball.radius > SCREEN_WIDTH) { ball.x = SCREEN_WIDTH - ball.radius; ball.vx *= -1; }
            if (ball.y - ball.radius < 0) { ball.y = ball.radius; ball.vy *= -1; }
            else if (ball.y + ball.radius > SCREEN_HEIGHT) { ball.y = SCREEN_HEIGHT - ball.radius; ball.vy *= -1; }
        }

        for (size_t i = 0; i < balls.size(); ++i) {
            for (size_t j = i + 1; j < balls.size(); ++j) {
                CollisionDetection(balls[i], balls[j]);
            }
        }

        SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, 1.0f);
        SDL_RenderClear(renderer);

        for (const auto& ball : balls) {
            DrawBall(renderer, ball);
        }

		// Render FPS, RAM, and CPU usage
        if (font) {
            SDL_Color textColor = { 255, 255, 255, 255 };
            SDL_Color hintColor = { 180, 180, 180, 255 };

            std::string fpsText = "FPS: " + std::to_string(fps);
            RenderText(renderer, font, fpsText, 10.0f, 10.0f, textColor);

            std::string ramText = "RAM: " + std::to_string(currentRamMB) + " MB";
            RenderText(renderer, font, ramText, 10.0f, 40.0f, textColor);

            // CPU Output
            char cpuBuffer[64];
            if (showSingleCoreView) {
                snprintf(cpuBuffer, sizeof(cpuBuffer), "CPU (Core %d Load): %.1f %%", currentCoreID, currentCpuUsage);
            }
            else {
                snprintf(cpuBuffer, sizeof(cpuBuffer), "CPU (Total System): %.1f %%", currentCpuUsage);
            }
            RenderText(renderer, font, cpuBuffer, 10.0f, 70.0f, textColor);

            RenderText(renderer, font, "[Press C: Switch View]", 10.0f, 100.0f, hintColor);
        }

        SDL_RenderPresent(renderer);

		// Update FPS and system metrics every second
        frameCount++;
        Uint64 currentTime = SDL_GetTicks();

        if (currentTime - lastTime >= 1000) {
            fps = frameCount;
            frameCount = 0;
            lastTime = currentTime;

            currentRamMB = GetProcessRAMUsageMB();
            currentCpuUsage = cpuTracker.Update(showSingleCoreView);
            currentCoreID = cpuTracker.GetCurrentCoreID();
        }
    }

	// Cleanup
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
