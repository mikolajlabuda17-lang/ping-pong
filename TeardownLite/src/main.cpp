#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>

// Very small 2D voxel-like heist game inspired by destruction sandbox mechanics.
// No external assets; no text rendering. UI is represented by simple shapes.

enum class CellType : uint8_t { Empty = 0, Solid = 1, Fire = 2, Valuable = 3, Computer = 4 };

struct Grid {
    int cols;
    int rows;
    int cellSize;
    std::vector<CellType> cells;

    Grid(int c, int r, int size) : cols(c), rows(r), cellSize(size), cells(c * r, CellType::Empty) {}

    inline CellType &at(int x, int y) { return cells[y * cols + x]; }
    bool inBounds(int x, int y) const { return x >= 0 && x < cols && y >= 0 && y < rows; }

    void fillSolidFloor() {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (y > rows * 2 / 3) cells[y * cols + x] = CellType::Solid; else cells[y * cols + x] = CellType::Empty;
            }
        }
    }

    void buildSimpleHouseAndComputer(int &compX, int &compY) {
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
        for (int x = hx0 + 6; x <= hx1 - 6; x += 10) {
            for (int y = hy0 + 4; y <= hy1 - 4; ++y) at(x, y) = CellType::Solid;
        }
        for (int y = hy1 - 3; y <= hy1; ++y) {
            for (int x = (hx0 + hx1) / 2 - 2; x <= (hx0 + hx1) / 2 + 2; ++x) at(x, y) = CellType::Empty;
        }
        // Place a computer block on a table inside the house
        compX = hx0 + 4;
        compY = hy0 + 3;
        at(compX, compY) = CellType::Computer;
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

struct Player { float x; float y; float speed; int radiusPx; };

enum class Tool { Hammer = 0, Extinguisher = 1 };

struct Mission { bool active=false, success=false, failed=false; int totalValuables=0, collectedValuables=0; int timeLeftMs=0; };

struct Progression { int score=0; bool extinguisherUnlocked=false; };

static void drawRect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c){ SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); SDL_Rect rc{ x,y,w,h }; SDL_RenderFillRect(r,&rc);} 

static std::string getUserDataDir() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string base = xdg && *xdg ? xdg : (std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/.local/share" : std::string("./"));
    std::string path = base + "/teardown-2d";
    mkdir(path.c_str(), 0755);
    return path;
}

static bool loadProgressSafe(Progression &out) {
    std::string dir = getUserDataDir();
    std::ifstream in(dir + "/save.txt");
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("score=", 0) == 0) out.score = std::stoi(line.substr(6));
        if (line.rfind("extinguisher_unlocked=", 0) == 0) out.extinguisherUnlocked = (line.substr(23) == "1");
    }
    return true;
}

static void saveProgressSafe(const Progression &p) {
    std::string dir = getUserDataDir();
    std::ofstream out(dir + "/save.txt", std::ios::trunc);
    out << "score=" << p.score << "\n";
    out << "extinguisher_unlocked=" << (p.extinguisherUnlocked ? 1 : 0) << "\n";
}

static bool isSolidAtPixel(const Grid &g,int px,int py){ int cx=px/g.cellSize, cy=py/g.cellSize; if(!g.inBounds(cx,cy)) return true; CellType t=g.cells[cy*g.cols+cx]; return (t==CellType::Solid); }

static void movePlayer(const Grid &g, Player &p, float dx,float dy,float dt){ float nx=p.x+dx*dt, ny=p.y+dy*dt; int r=p.radiusPx; int testX=(int)nx, topY=(int)p.y-r, bottomY=(int)p.y+r; bool blockX=isSolidAtPixel(g,testX-r,topY)||isSolidAtPixel(g,testX+r,topY)||isSolidAtPixel(g,testX-r,bottomY)||isSolidAtPixel(g,testX+r,bottomY); if(!blockX) p.x=nx; int testY=(int)ny, leftX=(int)p.x-r, rightX=(int)p.x+r; bool blockY=isSolidAtPixel(g,leftX,testY-r)||isSolidAtPixel(g,rightX,testY-r)||isSolidAtPixel(g,leftX,testY+r)||isSolidAtPixel(g,rightX,testY+r); if(!blockY) p.y=ny; }

static void drawGrid(SDL_Renderer *ren, const Grid &g){ for(int y=0;y<g.rows;++y){ for(int x=0;x<g.cols;++x){ CellType t=g.cells[y*g.cols+x]; if(t==CellType::Empty) continue; int px=x*g.cellSize, py=y*g.cellSize; if(t==CellType::Solid) drawRect(ren,px,py,g.cellSize,g.cellSize,SDL_Color{190,190,190,255}); else if(t==CellType::Fire) drawRect(ren,px,py,g.cellSize,g.cellSize,SDL_Color{255,120,40,255}); else if(t==CellType::Valuable) drawRect(ren,px,py,g.cellSize,g.cellSize,SDL_Color{235,200,0,255}); else if(t==CellType::Computer) drawRect(ren,px,py,g.cellSize,g.cellSize,SDL_Color{80,160,255,255}); } } }

static void spreadFire(Grid &g){ static std::mt19937 rng{std::random_device{}()}; std::uniform_int_distribution<int> chance(0,1000); std::vector<std::pair<int,int>> ignite; for(int y=1;y<g.rows-1;++y){ for(int x=1;x<g.cols-1;++x){ if(g.cells[y*g.cols+x]==CellType::Fire){ for(int oy=-1;oy<=1;++oy){ for(int ox=-1;ox<=1;++ox){ if(!ox&&!oy) continue; int nx=x+ox, ny=y+oy; auto &c=g.at(nx,ny); if(c==CellType::Solid && chance(rng)<2) ignite.emplace_back(nx,ny);} } if(chance(rng)<3) g.at(x,y)=CellType::Empty; } } } for(auto &p:ignite) g.at(p.first,p.second)=CellType::Fire; }

static void placeValuables(Grid &g,int count){ static std::mt19937 rng{std::random_device{}()}; std::uniform_int_distribution<int> cx(g.cols/4+2,g.cols*3/4-2), cy(g.rows/5+2,g.rows/2-2); int placed=0,guard=0; while(placed<count && guard<10000){ int x=cx(rng), y=cy(rng); if(g.at(x,y)==CellType::Empty){ g.at(x,y)=CellType::Valuable; placed++; } guard++; } }

static void startMission(Grid &g, Mission &m, Player &p, int &compX,int &compY, int valuables,int seconds){ for(auto &c:g.cells) c=CellType::Empty; g.fillSolidFloor(); g.buildSimpleHouseAndComputer(compX,compY); placeValuables(g,valuables); m={}; m.active=true; m.totalValuables=valuables; m.timeLeftMs=seconds*1000; p.x=g.cols*g.cellSize/2.0f; p.y=g.rows*g.cellSize*0.75f; }

static void collectIfOverlapping(Grid &g,const Player &p, Mission &m){ int r=p.radiusPx; int l=(int)(p.x-r), rt=(int)(p.x+r), t=(int)(p.y-r), b=(int)(p.y+r); for(int py=t;py<=b;py+=std::max(1,r)){ for(int px=l;px<=rt;px+=std::max(1,r)){ int cx=px/g.cellSize, cy=py/g.cellSize; if(!g.inBounds(cx,cy)) continue; if(g.at(cx,cy)==CellType::Valuable){ g.at(cx,cy)=CellType::Empty; m.collectedValuables++; } } } }

static void useHammer(Grid &g,const Player &p,int txPix,int tyPix,int radiusCells){ int px=(int)p.x/g.cellSize, py=(int)p.y/g.cellSize, tx=txPix/g.cellSize, ty=tyPix/g.cellSize; int dx=tx-px, dy=ty-py; if(dx*dx+dy*dy>36) return; g.circularApply(tx,ty,radiusCells,[&](int x,int y){ if(g.at(x,y)==CellType::Solid||g.at(x,y)==CellType::Fire) g.at(x,y)=CellType::Empty; }); }
static void useExt(Grid &g,const Player &p,int txPix,int tyPix,int radiusCells){ int px=(int)p.x/g.cellSize, py=(int)p.y/g.cellSize, tx=txPix/g.cellSize, ty=tyPix/g.cellSize; int dx=tx-px, dy=ty-py; if(dx*dx+dy*dy>400) return; g.circularApply(tx,ty,radiusCells+2,[&](int x,int y){ if(g.at(x,y)==CellType::Fire) g.at(x,y)=CellType::Empty; }); }

int main(int argc,char**argv){ (void)argc;(void)argv; if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ SDL_Log("SDL_Init Error: %s", SDL_GetError()); return 1; }
    const int windowWidth=960, windowHeight=640, cellSize=8, cols=windowWidth/cellSize, rows=windowHeight/cellSize;
    SDL_Window*win=SDL_CreateWindow("Heist Lite - 2D",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,windowWidth,windowHeight,SDL_WINDOW_SHOWN); if(!win){ SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer*ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC); if(!ren){ SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    Grid g(cols,rows,cellSize); g.fillSolidFloor();
    Player pl{ windowWidth/2.0f, rows*cellSize*0.75f, 160.0f, 6 };
    Tool tool=Tool::Hammer; int brush=2; bool running=true; Progression prog{}; loadProgressSafe(prog); Mission mis{}; auto last=std::chrono::steady_clock::now(); bool lmb=false; int compX=cols/2, compY=rows/3; bool computerOpen=false;

    // initial world
    g.buildSimpleHouseAndComputer(compX,compY);

    while(running){ SDL_Event e; while(SDL_PollEvent(&e)){ if(e.type==SDL_QUIT) running=false; if(e.type==SDL_KEYDOWN){ if(e.key.keysym.sym==SDLK_ESCAPE) running=false; if(!computerOpen){ if(e.key.keysym.sym==SDLK_1) tool=Tool::Hammer; if(e.key.keysym.sym==SDLK_2 && prog.extinguisherUnlocked) tool=Tool::Extinguisher; if(e.key.keysym.sym==SDLK_LEFTBRACKET && brush>1) --brush; if(e.key.keysym.sym==SDLK_RIGHTBRACKET && brush<8) ++brush; if(e.key.keysym.sym==SDLK_e){ // toggle computer if near
                        int pcx=(int)pl.x/g.cellSize, pcy=(int)pl.y/g.cellSize; int dx=pcx-compX, dy=pcy-compY; if(dx*dx+dy*dy<=25) computerOpen=true; }
                    } else { // computer UI keybinds
                        if(e.key.keysym.sym==SDLK_e) computerOpen=false;
                        if(e.key.keysym.sym==SDLK_1){ startMission(g,mis,pl,compX,compY,6,90); computerOpen=false; }
                        if(e.key.keysym.sym==SDLK_2){ startMission(g,mis,pl,compX,compY,10,75); computerOpen=false; }
                        if(e.key.keysym.sym==SDLK_3){ startMission(g,mis,pl,compX,compY,15,60); computerOpen=false; }
                    }
                }
                if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) lmb=true; if(e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT) lmb=false; }

        auto now=std::chrono::steady_clock::now(); float dt=std::chrono::duration<float>(now-last).count(); last=now;

        const Uint8*ks=SDL_GetKeyboardState(NULL); if(!computerOpen){ float dx=0,dy=0; if(ks[SDL_SCANCODE_A]) dx-=pl.speed; if(ks[SDL_SCANCODE_D]) dx+=pl.speed; if(ks[SDL_SCANCODE_W]) dy-=pl.speed; if(ks[SDL_SCANCODE_S]) dy+=pl.speed; movePlayer(g,pl,dx,dy,dt);} 

        int mx,my; SDL_GetMouseState(&mx,&my); if(lmb && !computerOpen){ if(tool==Tool::Hammer){ useHammer(g,pl,mx,my,brush); static std::mt19937 rng{std::random_device{}()}; std::uniform_int_distribution<int> ch(0,100); if(ch(rng)<3){ int tx=mx/g.cellSize, ty=my/g.cellSize; if(g.inBounds(tx,ty) && g.at(tx,ty)==CellType::Solid) g.at(tx,ty)=CellType::Fire; } } else if(tool==Tool::Extinguisher && prog.extinguisherUnlocked){ useExt(g,pl,mx,my,brush+1);} }

        spreadFire(g);

        if(mis.active && !mis.success && !mis.failed){ mis.timeLeftMs -= (int)(dt*1000.0f); collectIfOverlapping(g,pl,mis); if(mis.collectedValuables>=mis.totalValuables){ mis.success=true; mis.active=false; int timeBonus=std::max(0,mis.timeLeftMs/1000); int gained=mis.totalValuables*100+timeBonus; prog.score+=gained; if(!prog.extinguisherUnlocked && prog.score>=300) prog.extinguisherUnlocked=true; saveProgressSafe(prog);} else if(mis.timeLeftMs<=0){ mis.failed=true; mis.active=false; } }

        SDL_SetRenderDrawColor(ren,18,18,22,255); SDL_RenderClear(ren);
        drawGrid(ren,g);
        drawRect(ren,(int)pl.x-pl.radiusPx,(int)pl.y-pl.radiusPx,pl.radiusPx*2,pl.radiusPx*2,SDL_Color{100,220,255,255});

        // top bar
        SDL_Rect bar{10,10,windowWidth-20,60}; SDL_SetRenderDrawColor(ren,0,0,0,160); SDL_RenderFillRect(ren,&bar); SDL_SetRenderDrawColor(ren,80,80,80,255); SDL_RenderDrawRect(ren,&bar);
        drawRect(ren,20,20,24,24,SDL_Color{160,160,160,255}); drawRect(ren,25,25,8,14,SDL_Color{120,80,40,255}); SDL_Color extC=prog.extinguisherUnlocked?SDL_Color{200,60,60,255}:SDL_Color{80,40,40,255}; drawRect(ren,60,20,24,24,extC); drawRect(ren,66,18,12,6,extC);
        if(tool==Tool::Hammer){ SDL_SetRenderDrawColor(ren,255,255,255,200); SDL_Rect r{18,18,28,28}; SDL_RenderDrawRect(ren,&r);} else { SDL_SetRenderDrawColor(ren,255,255,255,200); SDL_Rect r{58,18,28,28}; SDL_RenderDrawRect(ren,&r);} 
        int remaining=std::max(0,mis.totalValuables-mis.collectedValuables); for(int i=0;i<std::min(remaining,10);++i) drawRect(ren,110+i*14,24,12,12,SDL_Color{235,200,0,255});
        if(mis.active){ float ratio=std::clamp(mis.timeLeftMs/(90.0f*1000.0f),0.0f,1.0f); int barW=(int)((windowWidth-240)*ratio); drawRect(ren,110,44,barW,12,SDL_Color{120,200,255,255}); } else if(mis.success){ drawRect(ren,110,44,windowWidth-240,12,SDL_Color{120,255,160,255}); } else if(mis.failed){ drawRect(ren,110,44,windowWidth-240,12,SDL_Color{255,120,120,255}); }
        int scoreBlocks=std::min(20,prog.score/50); for(int i=0;i<scoreBlocks;++i) drawRect(ren,windowWidth-30,18+i*6,8,4,SDL_Color{180,220,255,255});

        // Computer interaction hint
        int pcxPix=compX*cellSize, pcyPix=compY*cellSize; drawRect(ren,pcxPix, pcyPix-2, cellSize, 2, SDL_Color{80,160,255,200});
        int pcx=(int)pl.x/g.cellSize, pcy=(int)pl.y/g.cellSize; int ndx=pcx-compX, ndy=pcy-compY; if(ndx*ndx+ndy*ndy<=25 && !computerOpen){ SDL_Rect hint{pcxPix-6*cellSize, pcyPix-5*cellSize, 12*cellSize, 20}; SDL_SetRenderDrawColor(ren,20,20,40,200); SDL_RenderFillRect(ren,&hint);} 

        // Crosshair and brush
        int mx2,my2; SDL_GetMouseState(&mx2,&my2); drawRect(ren,mx2-1,my2-8,2,16,SDL_Color{255,100,100,255}); drawRect(ren,mx2-8,my2-1,16,2,SDL_Color{255,100,100,255});

        // Computer UI overlay
        if(computerOpen){ SDL_Rect panel{windowWidth/2-200, windowHeight/2-120, 400, 240}; SDL_SetRenderDrawColor(ren,10,10,20,230); SDL_RenderFillRect(ren,&panel); SDL_SetRenderDrawColor(ren,100,100,140,255); SDL_RenderDrawRect(ren,&panel);
            // Three mission boxes
            drawRect(ren,panel.x+20, panel.y+30, 360, 40, SDL_Color{40,80,140,255});
            drawRect(ren,panel.x+20, panel.y+90, 360, 40, SDL_Color{40,120,100,255});
            drawRect(ren,panel.x+20, panel.y+150,360, 40, SDL_Color{120,80,40,255});
            // Simple legend squares as placeholders for text (1/2/3)
            drawRect(ren,panel.x+26, panel.y+36, 28, 28, SDL_Color{220,220,220,255});
            drawRect(ren,panel.x+26, panel.y+96, 28, 28, SDL_Color{220,220,220,255});
            drawRect(ren,panel.x+26, panel.y+156,28, 28, SDL_Color{220,220,220,255});
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 0; }