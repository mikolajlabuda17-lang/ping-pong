#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <chrono>

struct Cell {
    bool filled;
};

struct Grid {
    int cols;
    int rows;
    int cellSize;
    std::vector<Cell> cells;

    Grid(int c, int r, int size) : cols(c), rows(r), cellSize(size), cells(c * r) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                bool startFilled = y > rows / 2 || (y > rows / 3 && (x % 4 != 0));
                at(x, y).filled = startFilled;
            }
        }
    }

    inline Cell &at(int x, int y) {
        return cells[y * cols + x];
    }

    void blast(int cx, int cy, int radius) {
        for (int y = cy - radius; y <= cy + radius; ++y) {
            if (y < 0 || y >= rows) continue;
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (x < 0 || x >= cols) continue;
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) {
                    at(x, y).filled = false;
                }
            }
        }
    }

    void place(int cx, int cy, int radius) {
        for (int y = cy - radius; y <= cy + radius; ++y) {
            if (y < 0 || y >= rows) continue;
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (x < 0 || x >= cols) continue;
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) {
                    at(x, y).filled = true;
                }
            }
        }
    }

    void settleOnce() {
        // Simple sand-like gravity: from bottom-1 to top
        for (int y = rows - 2; y >= 0; --y) {
            for (int x = 0; x < cols; ++x) {
                if (at(x, y).filled && !at(x, y + 1).filled) {
                    at(x, y + 1).filled = true;
                    at(x, y).filled = false;
                } else if (at(x, y).filled) {
                    // Try slide down-left/right
                    bool moved = false;
                    if (x > 0 && !at(x - 1, y + 1).filled) {
                        at(x - 1, y + 1).filled = true;
                        at(x, y).filled = false;
                        moved = true;
                    }
                    if (!moved && x < cols - 1 && !at(x + 1, y + 1).filled) {
                        at(x + 1, y + 1).filled = true;
                        at(x, y).filled = false;
                    }
                }
            }
        }
    }
};

static void drawRect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect{ x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    const int windowWidth = 960;
    const int windowHeight = 640;
    const int cellSize = 8;
    const int cols = windowWidth / cellSize;
    const int rows = windowHeight / cellSize;

    SDL_Window *window = SDL_CreateWindow(
        "Teardown Lite - 2D Destruction", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight, SDL_WINDOW_SHOWN
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Grid grid(cols, rows, cellSize);

    bool running = true;
    bool showHelp = true;
    int brush = 3;

    auto lastSettle = std::chrono::steady_clock::now();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_h) showHelp = !showHelp;
                if (e.key.keysym.sym == SDLK_LEFTBRACKET && brush > 1) brush--;
                if (e.key.keysym.sym == SDLK_RIGHTBRACKET && brush < 20) brush++;
                if (e.key.keysym.sym == SDLK_c) {
                    for (int y = 0; y < rows; ++y) for (int x = 0; x < cols; ++x) grid.at(x, y).filled = false;
                }
                if (e.key.keysym.sym == SDLK_r) {
                    for (int y = 0; y < rows; ++y) for (int x = 0; x < cols; ++x) grid.at(x, y).filled = (y > rows / 2);
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEMOTION) {
                if (e.type == SDL_MOUSEMOTION && (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) == 0) {
                    // no drawing
                } else {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    int cx = mx / cellSize;
                    int cy = my / cellSize;
                    if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEMOTION) {
                        if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) grid.blast(cx, cy, brush);
                        if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) grid.place(cx, cy, brush);
                    }
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSettle).count() > 20) {
            grid.settleOnce();
            lastSettle = now;
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderClear(renderer);

        // Draw terrain
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (grid.at(x, y).filled) {
                    int px = x * cellSize;
                    int py = y * cellSize;
                    drawRect(renderer, px, py, cellSize, cellSize, SDL_Color{200, 200, 200, 255});
                }
            }
        }

        // Draw crosshair
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        drawRect(renderer, mx - 1, my - 8, 2, 16, SDL_Color{255, 100, 100, 255});
        drawRect(renderer, mx - 8, my - 1, 16, 2, SDL_Color{255, 100, 100, 255});

        // Minimal on-screen help as lines
        if (showHelp) {
            // Simple help box
            SDL_Rect box{10, 10, 360, 88};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            SDL_RenderDrawRect(renderer, &box);
            // No text rendering to keep dependencies small, so just draw tiny blocks to indicate help presence
            for (int i = 0; i < 10; ++i) {
                drawRect(renderer, 20 + i * 8, 20, 6, 6, SDL_Color{120, 200, 255, 255});
            }
            // We cannot render fonts without SDL_ttf; rely on README for controls
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}