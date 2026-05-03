#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

// ================= 配置 =================
const int WIDTH = 40;
const int HEIGHT = 20;
const int MAX_LEVELS = 10;

// ================= 终端控制类 =================
class Terminal {
private:
    struct termios orig_termios;
    bool raw_mode = false;

public:
    void enableRawMode() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode = true;
    }

    void disableRawMode() {
        if (raw_mode) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            raw_mode = false;
        }
    }

    char readKey() {
        char c = 0;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            read(STDIN_FILENO, &c, 1);
        }
        return c;
    }

    void clear() {
        std::cout << "\033[2J\033[H" << std::flush;
    }

    void hideCursor() {
        std::cout << "\033[?25l" << std::flush;
    }

    void showCursor() {
        std::cout << "\033[?25h" << std::flush;
    }

    void setColor(int r, int g, int b) {
        std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
    }
    
    void resetColor() {
        std::cout << "\033[0m";
    }
};

// ================= 游戏实体 =================
enum TileType { EMPTY, WALL, WATER, LAVA, EXIT };

struct Player {
    int x, y;
    bool isFire;
    bool active;
};

// ================= 游戏主逻辑 =================
class Game {
private:
    Terminal term;
    int level;
    std::vector<std::vector<TileType>> map;
    Player fireBoy, iceGirl;
    bool gameOver;
    bool win;

public:
    Game() : level(1), gameOver(false), win(false) {
        term.enableRawMode();
        term.hideCursor();
        srand(time(NULL));
        resetLevel();
    }

    ~Game() {
        term.disableRawMode();
        term.showCursor();
        term.clear();
    }

    void resetLevel() {
        gameOver = false;
        win = false;
        map = std::vector<std::vector<TileType>>(HEIGHT, std::vector<TileType>(WIDTH, EMPTY));
        
        // 生成边界墙
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) {
                    map[y][x] = WALL;
                } else {
                    // 随机生成障碍 (难度随层数增加)
                    int difficulty = level * 2; 
                    if (rand() % 100 < difficulty) {
                        map[y][x] = (rand() % 2 == 0) ? WATER : LAVA;
                    }
                }
            }
        }

        // 放置玩家 (确保出生点安全)
        fireBoy = {2, 2, true, true};
        iceGirl = {WIDTH - 3, 2, false, false};
        map[fireBoy.y][fireBoy.x] = EMPTY;
        map[fireBoy.y][fireBoy.x + 1] = EMPTY;
        map[iceGirl.y][iceGirl.x] = EMPTY;
        map[iceGirl.y][iceGirl.x - 1] = EMPTY;

        // 放置出口 (右下角，确保周围安全)
        map[HEIGHT - 2][WIDTH - 2] = EXIT;
        map[HEIGHT - 2][WIDTH - 3] = EMPTY;
        map[HEIGHT - 3][WIDTH - 2] = EMPTY;
    }

    void toggleControl() {
        fireBoy.active = !fireBoy.active;
        iceGirl.active = !fireBoy.active;
    }

    void processInput() {
        char key = term.readKey();
        if (key == 0) return;

        // Tab 切换控制
        if (key == 9) {
            toggleControl();
            return;
        }
        // 退出游戏
        if (key == 'q') { 
            gameOver = true; 
            win = false; 
            return; 
        }

        Player* p = fireBoy.active ? &fireBoy : &iceGirl;
        int dx = 0, dy = 0;

        // 方向键处理 (ANSI 转义序列)
        if (key == 27) { 
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[1] == 'A') dy = -1;
                else if (seq[1] == 'B') dy = 1;
                else if (seq[1] == 'C') dx = 1;
                else if (seq[1] == 'D') dx = -1;
            }
        } else {
            // WASD 支持
            if (key == 'w' || key == 'W') dy = -1;
            else if (key == 's' || key == 'S') dy = 1;
            else if (key == 'd' || key == 'D') dx = 1;
            else if (key == 'a' || key == 'A') dx = -1;
        }

        if (dx != 0 || dy != 0) {
            int newX = p->x + dx;
            int newY = p->y + dy;
            TileType tile = map[newY][newX];

            if (tile != WALL) {
                // 属性检测
                if (p->isFire && tile == WATER) { 
                    gameOver = true; 
                } else if (!p->isFire && tile == LAVA) { 
                    gameOver = true; 
                } else {
                    p->x = newX;
                    p->y = newY;
                    
                    // 胜利条件：两人都到达出口
                    if (tile == EXIT) {
                        Player* other = fireBoy.active ? &iceGirl : &fireBoy;
                        if (other->x == WIDTH - 2 && other->y == HEIGHT - 2) {
                            win = true;
                            gameOver = true;
                        }
                    }
                }
            }
        }
    }

    void draw() {
        term.clear();
        std::cout << "=== SSH Ice & Fire === Level: " << level 
                  << " | Tab: Switch | Q: Quit | WASD/Arrows: Move\n\n";

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                bool isFirePos = (fireBoy.x == x && fireBoy.y == y);
                bool isIcePos = (iceGirl.x == x && iceGirl.y == y);

                if (isFirePos) {
                    term.setColor(255, 80, 80);
                    std::cout << (fireBoy.active ? "@" : "F");
                    term.resetColor();
                } else if (isIcePos) {
                    term.setColor(80, 80, 255);
                    std::cout << (iceGirl.active ? "@" : "I");
                    term.resetColor();
                } else {
                    TileType t = map[y][x];
                    if (t == WALL) {
                        term.setColor(120, 120, 120);
                        std::cout << "#";
                    } else if (t == WATER) {
                        term.setColor(50, 150, 255);
                        std::cout << "~";
                    } else if (t == LAVA) {
                        term.setColor(255, 120, 50);
                        std::cout << "^";
                    } else if (t == EXIT) {
                        term.setColor(50, 255, 50);
                        std::cout << "E";
                    } else {
                        std::cout << " ";
                    }
                    term.resetColor();
                }
            }
            std::cout << "\n";
        }
    }

    void run() {
        while (!gameOver) {
            processInput();
            draw();
            usleep(50000);
        }

        term.showCursor();
        if (win) {
            term.clear();
            std::cout << "\n*** LEVEL " << level << " COMPLETE! ***\n\n";
            level++;
            if (level > MAX_LEVELS) {
                std::cout << "CONGRATULATIONS! YOU BEAT ALL " << MAX_LEVELS << " LEVELS!\n";
                return;
            } else {
                std::cout << "Get ready for Level " << level << "...\n";
                sleep(2);
                term.hideCursor();
                resetLevel();
                gameOver = false;
                run();
            }
        } else {
            term.clear();
            std::cout << "\n*** GAME OVER ***\n";
            std::cout << "You touched the wrong element!\n\n";
            std::cout << "Press 'r' to retry | Press 'q' to quit\n";
            
            char c;
            read(STDIN_FILENO, &c, 1);
            if (c == 'r' || c == 'R') {
                term.hideCursor();
                resetLevel();
                gameOver = false;
                run();
            }
        }
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}
