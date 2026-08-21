#include <iostream>
#include "MainHeader.h"
#include <vector>
#include <cmath>
#include <SDL3/SDL.h>
#include <random>
#include <SDL3/SDL_ttf.h>
#include <string>

// Include Windows-specific headers for System Metrics
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// Helper to convert FILETIME to 64-bit integer
#ifdef _WIN32
static ULONGLONG FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

// RAM usage function for Windows
size_t GetProcessRAMUsageMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
        return info.WorkingSetSize / (1024 * 1024); // Convert bytes to MB
    }
#endif
    return 0;
}

// CPU Usage tracker for Windows
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
        Update(); // Pre-fill initial values
#endif
    }

    float Update() {
#ifdef _WIN32
        FILETIME ftime, fsys, fuser;
        FILETIME fcreation, fexit, fkernel, fuserProc;

        GetSystemTimeAsFileTime(&ftime);
        GetSystemStoreTime(&fsys, &fuser); // System Total Time
        GetProcessTimes(GetCurrentProcess(), &fcreation, &fexit, &fkernel, &fuserProc); // Process Time

        ULONGLONG currentSystemTime = FileTimeToUint64(ftime);
        ULONGLONG currentKernelTime = FileTimeToUint64(fkernel);
        ULONGLONG currentUserTime = FileTimeToUint64(fuserProc);

        ULONGLONG sysDelta = currentSystemTime - lastSystemTime;
        ULONGLONG procDelta = (currentKernelTime - lastKernelTime) + (currentUserTime - lastUserTime);

        lastSystemTime = currentSystemTime;
        lastKernelTime = currentKernelTime;
        lastUserTime = currentUserTime;

        if (sysDelta > 0) {
            // Divide by core count to get process CPU percentage relative to entire system
            return (static_cast<float>(procDelta) / sysDelta / numProcessors) * 100.0f;
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

// Function to draw a ball
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

// Collision detection function of two balls
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

// Function to render text
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
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Initialize TTF
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window
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

    // Create renderer
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

    // Main loop variables
    bool running = true;
    SDL_Event event;

    // Performance variables
    size_t currentRamMB = 0;
    float currentCpuUsage = 0.0f;
    CPUUsageTracker cpuTracker;

    // Set random seed
    std::random_device rd;
    std::uniform_real_distribution<float> distrib(0.0f, 1.0f);

    // Create balls
    std::vector<Ball> balls;
    int numBalls = 1000;

    for (int i = 0; i < numBalls; ++i) {
        float radius = distrib(rd) * 3.0f + 1.0f;
        float x = distrib(rd) * (SCREEN_WIDTH - 2 * radius) + radius;
        float y = distrib(rd) * (SCREEN_HEIGHT - 2 * radius) + radius;
        float vx = distrib(rd) - 0.5f;
        float vy = distrib(rd) - 0.5f;
        SDL_Color color = { static_cast<Uint8>(distrib(rd) * 255), static_cast<Uint8>(distrib(rd) * 255), static_cast<Uint8>(distrib(rd) * 255), 255 };
        balls.push_back({ x, y, vx, vy, radius, color });
    }

    // FPS
    Uint64 lastTime = SDL_GetTicks();
    int frameCount = 0;
    int fps = 0;

    // Main loop
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Update ball positions and handle wall collisions
        for (auto& ball : balls) {
            ball.x += ball.vx;
            ball.y += ball.vy;

            if (ball.x - ball.radius < 0) { ball.x = ball.radius; ball.vx *= -1; }
            else if (ball.x + ball.radius > SCREEN_WIDTH) { ball.x = SCREEN_WIDTH - ball.radius; ball.vx *= -1; }
            if (ball.y - ball.radius < 0) { ball.y = ball.radius; ball.vy *= -1; }
            else if (ball.y + ball.radius > SCREEN_HEIGHT) { ball.y = SCREEN_HEIGHT - ball.radius; ball.vy *= -1; }
        }

        // Collision detection
        for (size_t i = 0; i < balls.size(); ++i) {
            for (size_t j = i + 1; j < balls.size(); ++j) {
                CollisionDetection(balls[i], balls[j]);
            }
        }

        // Clear the screen
        SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, 1.0f);
        SDL_RenderClear(renderer);

        // Render balls
        for (const auto& ball : balls) {
            DrawBall(renderer, ball);
        }

        // Render Performance Text
        if (font) {
            SDL_Color textColor = { 255, 255, 255, 255 };

            std::string fpsText = "FPS: " + std::to_string(fps);
            RenderText(renderer, font, fpsText, 10.0f, 10.0f, textColor);

            // RAM
            std::string ramText = "RAM: " + std::to_string(currentRamMB) + " MB";
            RenderText(renderer, font, ramText, 10.0f, 40.0f, textColor);

            // CPU
            char cpuBuffer[32];
            snprintf(cpuBuffer, sizeof(cpuBuffer), "CPU: %.1f %%", currentCpuUsage);
            RenderText(renderer, font, cpuBuffer, 10.0f, 70.0f, textColor);
        }

        // Present the rendered frame
        SDL_RenderPresent(renderer);

        // FPS & Metrics calculation (once per second)
        frameCount++;
        Uint64 currentTime = SDL_GetTicks();

        if (currentTime - lastTime >= 1000) {
            fps = frameCount;
            frameCount = 0;
            lastTime = currentTime;

            currentRamMB = GetProcessRAMUsageMB();
            currentCpuUsage = cpuTracker.Update();
        }
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
