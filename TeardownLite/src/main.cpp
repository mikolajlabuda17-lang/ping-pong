#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

// Very small 2D voxel-like heist game inspired by destruction sandbox mechanics.
// No external assets; no text rendering. UI is represented by simple shapes.

enum class CellType : uint8_t { Empty = 0, Solid = 1, Fire = 2, Valuable = 3 };

struct Grid {
    int cols;
    int rows;
    int cellSize;
    std::vector<CellType> cells;

    Grid(int c, int r, int size) : cols(c), rows(r), cellSize(size), cells(c * r, CellType::Empty) {}

    inline CellType &at(int x, int y) {
        return cells[y * cols + x];
    }

    bool inBounds(int x, int y) const { return x >= 0 && x < cols && y >= 0 && y < rows; }

    void fillSolidFloor() {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (y > rows * 2 / 3) {
                    cells[y * cols + x] = CellType::Solid;
                } else {
                    cells[y * cols + x] = CellType::Empty;
                }
            }
        }
    }

    void buildSimpleHouse() {
        // Simple rectangle house in upper half
        int hx0 = cols / 4;
        int hx1 = cols * 3 / 4;
        int hy0 = rows / 5;
        int hy1 = rows / 2;
        for (int y = hy0; y <= hy1; ++y) {
            for (int x = hx0; x <= hx1; ++x) {
                bool wall = (x == hx0 || x == hx1 || y == hy0 || y == hy1);
                if (wall) at(x, y) = CellType::Solid;
            }
        }
        // A few inner pillars
        for (int x = hx0 + 6; x <= hx1 - 6; x += 10) {
            for (int y = hy0 + 4; y <= hy1 - 4; ++y) {
                at(x, y) = CellType::Solid;
            }
        }
        // Add a door opening
        for (int y = hy1 - 3; y <= hy1; ++y) {
            for (int x = (hx0 + hx1) / 2 - 2; x <= (hx0 + hx1) / 2 + 2; ++x) {
                at(x, y) = CellType::Empty;
            }
        }
    }

    void circularApply(int cx, int cy, int radius, const std::function<void(int,int)> &fn) {
        for (int y = cy - radius; y <= cy + radius; ++y) {
            if (y < 0 || y >= rows) continue;
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (x < 0 || x >= cols) continue;
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) fn(x, y);
            }
        }
    }
};

struct Player {
    float x;
    float y;
    float speed;
    int radiusPx;
};

enum class Tool { Hammer = 0, Extinguisher = 1 };

struct Mission {
    bool active = false;
    bool success = false;
    bool failed = false;
    int totalValuables = 0;
    int collectedValuables = 0;
    int timeLeftMs = 0; // countdown
};

struct Progression {
    int score = 0;
    bool extinguisherUnlocked = false;
};

static void drawRect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect{ x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}

static bool loadProgress(const std::string &path, Progression &out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("score=", 0) == 0) out.score = std::stoi(line.substr(6));
        if (line.rfind("extinguisher_unlocked=", 0) == 0) out.extinguisherUnlocked = (line.substr(23) == "1");
    }
    return true;
}

static void saveProgress(const std::string &path, const Progression &p) {
    std::ofstream out(path, std::ios::trunc);
    out << "score=" << p.score << "\n";
    out << "extinguisher_unlocked=" << (p.extinguisherUnlocked ? 1 : 0) << "\n";
}

static bool isSolidAtPixel(const Grid &grid, int px, int py) {
    int cx = px / grid.cellSize;
    int cy = py / grid.cellSize;
    if (!grid.inBounds(cx, cy)) return true; // outside treated as solid bounds
    return grid.cells[cy * grid.cols + cx] == CellType::Solid;
}

static void movePlayerWithCollision(const Grid &grid, Player &player, float dx, float dy, float dt) {
    float nx = player.x + dx * dt;
    float ny = player.y + dy * dt;
    int r = player.radiusPx;
    // Horizontal
    int testX = (int)nx;
    int topY = (int)player.y - r;
    int bottomY = (int)player.y + r;
    bool blockedX = isSolidAtPixel(grid, testX - r, topY) || isSolidAtPixel(grid, testX + r, topY)
                  || isSolidAtPixel(grid, testX - r, bottomY) || isSolidAtPixel(grid, testX + r, bottomY);
    if (!blockedX) player.x = nx;

    // Vertical
    int testY = (int)ny;
    int leftX = (int)player.x - r;
    int rightX = (int)player.x + r;
    bool blockedY = isSolidAtPixel(grid, leftX, testY - r) || isSolidAtPixel(grid, rightX, testY - r)
                  || isSolidAtPixel(grid, leftX, testY + r) || isSolidAtPixel(grid, rightX, testY + r);
    if (!blockedY) player.y = ny;
}

static void drawGrid(SDL_Renderer *renderer, const Grid &grid) {
    for (int y = 0; y < grid.rows; ++y) {
        for (int x = 0; x < grid.cols; ++x) {
            CellType t = grid.cells[y * grid.cols + x];
            if (t == CellType::Empty) continue;
            int px = x * grid.cellSize;
            int py = y * grid.cellSize;
            if (t == CellType::Solid) {
                drawRect(renderer, px, py, grid.cellSize, grid.cellSize, SDL_Color{190, 190, 190, 255});
            } else if (t == CellType::Fire) {
                drawRect(renderer, px, py, grid.cellSize, grid.cellSize, SDL_Color{255, 120, 40, 255});
            } else if (t == CellType::Valuable) {
                drawRect(renderer, px, py, grid.cellSize, grid.cellSize, SDL_Color{235, 200, 0, 255});
            }
        }
    }
}

static void spreadFire(Grid &grid) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> chance(0, 1000);
    std::vector<std::pair<int,int>> ignite;
    for (int y = 1; y < grid.rows - 1; ++y) {
        for (int x = 1; x < grid.cols - 1; ++x) {
            if (grid.cells[y * grid.cols + x] == CellType::Fire) {
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) continue;
                        int nx = x + ox, ny = y + oy;
                        CellType &c = grid.at(nx, ny);
                        if (c == CellType::Solid && chance(rng) < 2) ignite.emplace_back(nx, ny);
                    }
                }
                // Fire decays occasionally
                if (chance(rng) < 3) grid.at(x, y) = CellType::Empty;
            }
        }
    }
    for (auto &p : ignite) grid.at(p.first, p.second) = CellType::Fire;
}

static void placeValuables(Grid &grid, int count) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> cxDist(grid.cols / 4 + 2, grid.cols * 3 / 4 - 2);
    std::uniform_int_distribution<int> cyDist(grid.rows / 5 + 2, grid.rows / 2 - 2);
    int placed = 0;
    int guard = 0;
    while (placed < count && guard < 10000) {
        int x = cxDist(rng);
        int y = cyDist(rng);
        if (grid.at(x, y) == CellType::Empty) {
            grid.at(x, y) = CellType::Valuable;
            placed++;
        }
        guard++;
    }
}

static void startMission(Grid &grid, Mission &mission, Player &player, int valuables, int seconds) {
    // Reset world
    for (auto &c : grid.cells) c = CellType::Empty;
    grid.fillSolidFloor();
    grid.buildSimpleHouse();
    placeValuables(grid, valuables);

    mission.active = true;
    mission.success = false;
    mission.failed = false;
    mission.totalValuables = valuables;
    mission.collectedValuables = 0;
    mission.timeLeftMs = seconds * 1000;

    // Player spawn near bottom center
    player.x = grid.cols * grid.cellSize / 2.0f;
    player.y = grid.rows * grid.cellSize * 0.75f;
}

static void collectIfOverlapping(Grid &grid, const Player &player, Mission &mission) {
    int r = player.radiusPx;
    int left = (int)(player.x - r);
    int right = (int)(player.x + r);
    int top = (int)(player.y - r);
    int bottom = (int)(player.y + r);
    for (int py = top; py <= bottom; py += std::max(1, r)) {
        for (int px = left; px <= right; px += std::max(1, r)) {
            int cx = px / grid.cellSize;
            int cy = py / grid.cellSize;
            if (!grid.inBounds(cx, cy)) continue;
            if (grid.at(cx, cy) == CellType::Valuable) {
                grid.at(cx, cy) = CellType::Empty;
                mission.collectedValuables++;
            }
        }
    }
}

static void useHammer(Grid &grid, const Player &player, int targetX, int targetY, int radiusCells) {
    // Only allow usage near player (melee)
    int px = (int)player.x / grid.cellSize;
    int py = (int)player.y / grid.cellSize;
    int tx = targetX / grid.cellSize;
    int ty = targetY / grid.cellSize;
    int dx = tx - px;
    int dy = ty - py;
    if (dx * dx + dy * dy > 36) return; // too far (>6 cells)
    grid.circularApply(tx, ty, radiusCells, [&](int x, int y){
        if (grid.at(x, y) == CellType::Solid) grid.at(x, y) = CellType::Empty;
        if (grid.at(x, y) == CellType::Fire) grid.at(x, y) = CellType::Empty;
    });
}

static void useExtinguisher(Grid &grid, const Player &player, int targetX, int targetY, int radiusCells) {
    // Spray can reach further
    int px = (int)player.x / grid.cellSize;
    int py = (int)player.y / grid.cellSize;
    int tx = targetX / grid.cellSize;
    int ty = targetY / grid.cellSize;
    int dx = tx - px;
    int dy = ty - py;
    if (dx * dx + dy * dy > 20 * 20) return; // range gate
    grid.circularApply(tx, ty, radiusCells + 2, [&](int x, int y){
        if (grid.at(x, y) == CellType::Fire) grid.at(x, y) = CellType::Empty;
    });
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
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
        "Heist Lite - 2D Destruction", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
    grid.fillSolidFloor();

    Player player{ windowWidth / 2.0f, rows * cellSize * 0.75f, 160.0f, 6 };

    Tool currentTool = Tool::Hammer;
    int brush = 2;
    bool running = true;

    Progression progression{};
    loadProgress("save.txt", progression);

    Mission mission{};

    auto lastTick = std::chrono::steady_clock::now();
    bool mouseDownLeft = false;

    // Start initial mission
    startMission(grid, mission, player, 6, 90);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_1) currentTool = Tool::Hammer;
                if (e.key.keysym.sym == SDLK_2 && progression.extinguisherUnlocked) currentTool = Tool::Extinguisher;
                if (e.key.keysym.sym == SDLK_LEFTBRACKET && brush > 1) brush--;
                if (e.key.keysym.sym == SDLK_RIGHTBRACKET && brush < 8) brush++;
                if (e.key.keysym.sym == SDLK_n) startMission(grid, mission, player, 6 + (progression.score / 300), 90);
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseDownLeft = true;
            }
            if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseDownLeft = false;
            }
        }

        // Timing
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        // Input for movement
        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        float dx = 0.0f, dy = 0.0f;
        if (keystate[SDL_SCANCODE_A]) dx -= player.speed;
        if (keystate[SDL_SCANCODE_D]) dx += player.speed;
        if (keystate[SDL_SCANCODE_W]) dy -= player.speed;
        if (keystate[SDL_SCANCODE_S]) dy += player.speed;
        movePlayerWithCollision(grid, player, dx, dy, dt);

        // Tool usage
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mouseDownLeft) {
            if (currentTool == Tool::Hammer) {
                useHammer(grid, player, mx, my, brush);
                // Hammer can ignite a few blocks randomly to encourage extinguisher later
                static std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> chance(0, 100);
                if (chance(rng) < 3) {
                    int tx = mx / grid.cellSize;
                    int ty = my / grid.cellSize;
                    if (grid.inBounds(tx, ty) && grid.at(tx, ty) == CellType::Solid) grid.at(tx, ty) = CellType::Fire;
                }
            } else if (currentTool == Tool::Extinguisher && progression.extinguisherUnlocked) {
                useExtinguisher(grid, player, mx, my, brush + 1);
            }
        }

        // Update grid dynamics
        spreadFire(grid);

        // Mission logic
        if (mission.active && !mission.success && !mission.failed) {
            mission.timeLeftMs -= (int)(dt * 1000.0f);
            collectIfOverlapping(grid, player, mission);
            if (mission.collectedValuables >= mission.totalValuables) {
                mission.success = true;
                mission.active = false;
                int timeBonus = std::max(0, mission.timeLeftMs / 1000);
                int gained = mission.totalValuables * 100 + timeBonus;
                progression.score += gained;
                if (!progression.extinguisherUnlocked && progression.score >= 300) progression.extinguisherUnlocked = true;
                saveProgress("save.txt", progression);
            } else if (mission.timeLeftMs <= 0) {
                mission.failed = true;
                mission.active = false;
            }
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderClear(renderer);

        // World
        drawGrid(renderer, grid);

        // Player (cyan circle approximated by a filled square)
        drawRect(renderer, (int)player.x - player.radiusPx, (int)player.y - player.radiusPx,
                 player.radiusPx * 2, player.radiusPx * 2, SDL_Color{100, 220, 255, 255});

        // UI panel (top bar)
        SDL_Rect bar{10, 10, windowWidth - 20, 60};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, &bar);
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderDrawRect(renderer, &bar);

        // Tool indicators
        // Hammer icon
        drawRect(renderer, 20, 20, 24, 24, SDL_Color{160, 160, 160, 255});
        drawRect(renderer, 25, 25, 8, 14, SDL_Color{120, 80, 40, 255});
        // Extinguisher icon (red can)
        SDL_Color extinguisherColor = progression.extinguisherUnlocked ? SDL_Color{200, 60, 60, 255} : SDL_Color{80, 40, 40, 255};
        drawRect(renderer, 60, 20, 24, 24, extinguisherColor);
        drawRect(renderer, 66, 18, 12, 6, extinguisherColor);

        // Selected tool highlight
        if (currentTool == Tool::Hammer) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            SDL_Rect r{18, 18, 28, 28};
            SDL_RenderDrawRect(renderer, &r);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            SDL_Rect r{58, 18, 28, 28};
            SDL_RenderDrawRect(renderer, &r);
        }

        // Mission progress: draw valuables left as small yellow boxes
        int remaining = std::max(0, mission.totalValuables - mission.collectedValuables);
        for (int i = 0; i < std::min(remaining, 10); ++i) {
            drawRect(renderer, 110 + i * 14, 24, 12, 12, SDL_Color{235, 200, 0, 255});
        }

        // Timer bar
        if (mission.active) {
            float ratio = std::clamp(mission.timeLeftMs / (90.0f * 1000.0f), 0.0f, 1.0f);
            int barW = (int)((windowWidth - 240) * ratio);
            drawRect(renderer, 110, 44, barW, 12, SDL_Color{120, 200, 255, 255});
        } else if (mission.success) {
            drawRect(renderer, 110, 44, windowWidth - 240, 12, SDL_Color{120, 255, 160, 255});
        } else if (mission.failed) {
            drawRect(renderer, 110, 44, windowWidth - 240, 12, SDL_Color{255, 120, 120, 255});
        }

        // Score meter (right)
        int scoreBlocks = std::min(20, progression.score / 50);
        for (int i = 0; i < scoreBlocks; ++i) {
            drawRect(renderer, windowWidth - 30, 18 + i * 6, 8, 4, SDL_Color{180, 220, 255, 255});
        }

        // Crosshair and brush preview
        drawRect(renderer, mx - 1, my - 8, 2, 16, SDL_Color{255, 100, 100, 255});
        drawRect(renderer, mx - 8, my - 1, 16, 2, SDL_Color{255, 100, 100, 255});
        // Brush preview as faint square
        int preview = std::max(1, brush) * cellSize;
        drawRect(renderer, (mx / cellSize - brush) * cellSize, (my / cellSize - brush) * cellSize, preview * 2, 2, SDL_Color{80, 80, 120, 160});
        drawRect(renderer, (mx / cellSize - brush) * cellSize, (my / cellSize + brush) * cellSize, preview * 2, 2, SDL_Color{80, 80, 120, 160});
        drawRect(renderer, (mx / cellSize - brush) * cellSize, (my / cellSize - brush) * cellSize, 2, preview * 2, SDL_Color{80, 80, 120, 160});
        drawRect(renderer, (mx / cellSize + brush) * cellSize, (my / cellSize - brush) * cellSize, 2, preview * 2, SDL_Color{80, 80, 120, 160});

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}