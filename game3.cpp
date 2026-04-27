#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <algorithm>
#include <sstream> // 新增：用于高效构建字符串流

// ================= 配置 =================
const int WIDTH = 60;
const int HEIGHT = 25;
const int MAX_LEVELS = 20;
const int ANIMATION_FRAMES = 15;

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

    // 修改：只将光标移回左上角，不再擦除全屏 (2J)，这是消除闪烁的关键
    void resetCursor() {
        std::cout << "\033[H" << std::flush;
    }

    void clearScreen() {
        std::cout << "\033[2J\033[H" << std::flush;
    }

    void hideCursor() {
        std::cout << "\033[?25l" << std::flush;
    }

    void showCursor() {
        std::cout << "\033[?25h" << std::flush;
    }

    // 修改：将颜色控制改为返回字符串，以便拼接到缓冲区
    std::string getColorStr(int r, int g, int b) {
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    std::string getBgColorStr(int r, int g, int b) {
        return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }
    
    std::string getResetStr() {
        return "\033[0m";
    }
};

// ================= 游戏实体 =================
enum TileType { 
    EMPTY, WALL, WATER, LAVA, EXIT, 
    SPIKE, DOOR_CLOSED, DOOR_OPEN, 
    SWITCH, TELEPORT_IN, TELEPORT_OUT,
    ICE_FLOOR, SAND_FLOOR
};

enum SwitchState { OFF, ON };

struct Switch {
    int x, y;
    int linkedDoorX, linkedDoorY;
    SwitchState state;
};

struct Teleport {
    int inX, inY, outX, outY;
};

struct Player {
    int x, y;
    int prevX, prevY;
    bool isFire;
    bool active;
    bool isDead;
};

// ================= 游戏主逻辑 =================
class Game {
private:
    Terminal term;
    int level;
    int score;
    std::vector<std::vector<TileType>> map;
    std::vector<Switch> switches;
    std::vector<Teleport> teleports;
    Player fireBoy, iceGirl;
    bool gameOver;
    bool win;
    bool levelComplete;

public:
    Game() : level(1), score(0), gameOver(false), win(false), levelComplete(false) {
        term.enableRawMode();
        term.hideCursor();
        srand(time(NULL));
        generateLevel();
    }

    ~Game() {
        term.disableRawMode();
        term.showCursor();
        term.clearScreen();
    }

    void generateLevel() {
        // --- 修复核心：清空输入缓冲区 ---
        // 丢弃在过场动画或上一关末尾产生的残留按键
        tcflush(STDIN_FILENO, TCIFLUSH); 

        gameOver = false;
        win = false;
        levelComplete = false;
        switches.clear();
        teleports.clear();
        map = std::vector<std::vector<TileType>>(HEIGHT, std::vector<TileType>(WIDTH, EMPTY));
        
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) {
                    map[y][x] = WALL;
                }
            }
        }

        int complexity = level * 3;
        int obstacleDensity = 5 + level;
        
        for (int i = 0; i < complexity + 5; i++) {
            int platY = rand() % (HEIGHT - 4) + 2;
            int platX = rand() % (WIDTH - 10) + 3;
            int platLen = rand() % 8 + 3;
            for (int j = 0; j < platLen && platX + j < WIDTH - 1; j++) {
                if (map[platY][platX + j] == EMPTY) map[platY][platX + j] = WALL;
            }
        }

        for (int y = 2; y < HEIGHT - 2; y++) {
            for (int x = 2; x < WIDTH - 2; x++) {
                if (map[y][x] == EMPTY && rand() % 100 < obstacleDensity) {
                    int obstacleType = rand() % 100;
                    if (obstacleType < 30) map[y][x] = WATER;
                    else if (obstacleType < 55) map[y][x] = LAVA;
                    else if (obstacleType < 70 && level >= 2) map[y][x] = SPIKE;
                    else if (obstacleType < 80 && level >= 3) map[y][x] = ICE_FLOOR;
                    else if (obstacleType < 90 && level >= 4) map[y][x] = SAND_FLOOR;
                }
            }
        }

        if (level >= 2) {
            int numSwitches = std::min(level / 2, 3);
            for (int i = 0; i < numSwitches; i++) {
                int sx, sy, dx, dy;
                do { sx = rand() % (WIDTH - 4) + 2; sy = rand() % (HEIGHT - 4) + 2; } while (map[sy][sx] != EMPTY);
                do { dx = rand() % (WIDTH - 4) + 2; dy = rand() % (HEIGHT - 4) + 2; } while (map[dy][dx] != WALL);
                switches.push_back({sx, sy, dx, dy, OFF});
                map[sy][sx] = SWITCH;
                map[dy][dx] = DOOR_CLOSED;
            }
        }

        if (level >= 5) {
            int tx1 = rand() % (WIDTH - 4) + 2; int ty1 = rand() % (HEIGHT - 4) + 2;
            int tx2 = rand() % (WIDTH - 4) + 2; int ty2 = rand() % (HEIGHT - 4) + 2;
            if (map[ty1][tx1] == EMPTY && map[ty2][tx2] == EMPTY) {
                map[ty1][tx1] = TELEPORT_IN; map[ty2][tx2] = TELEPORT_OUT;
                teleports.push_back({tx1, ty1, tx2, ty2});
            }
        }

        fireBoy = {2, 2, 2, 2, true, true, false};
        iceGirl = {WIDTH - 3, 2, WIDTH - 3, 2, false, false, false};
        map[HEIGHT - 2][WIDTH - 2] = EXIT;
    }

    void toggleControl() {
        fireBoy.active = !fireBoy.active;
        iceGirl.active = !fireBoy.active;
    }

    void tryActivateSwitch(int x, int y) {
        for (auto& sw : switches) {
            if (sw.x == x && sw.y == y && sw.state == OFF) {
                sw.state = ON;
                map[sw.linkedDoorY][sw.linkedDoorX] = DOOR_OPEN;
                score += 50;
            }
        }
    }

    void tryTeleport(int& x, int& y) {
        for (auto& tp : teleports) {
            if (tp.inX == x && tp.inY == y) {
                x = tp.outX; y = tp.outY;
                score += 25; break;
            }
        }
    }

    void processInput() {
        char key = term.readKey();
        if (key == 0) return;
        if (key == 9) { toggleControl(); return; }
        if (key == 'q' || key == 'Q') { gameOver = true; return; }

        Player* p = fireBoy.active ? &fireBoy : &iceGirl;
        p->prevX = p->x; p->prevY = p->y;
        int dx = 0, dy = 0;

        if (key == 27) { 
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[1] == 'A') dy = -1; else if (seq[1] == 'B') dy = 1;
                else if (seq[1] == 'C') dx = 1; else if (seq[1] == 'D') dx = -1;
            }
        } else {
            if (key == 'w' || key == 'W') dy = -1; else if (key == 's' || key == 'S') dy = 1;
            else if (key == 'd' || key == 'D') dx = 1; else if (key == 'a' || key == 'A') dx = -1;
        }

        if (dx != 0 || dy != 0) {
            int newX = p->x + dx; int newY = p->y + dy;
            if (newX < 0 || newX >= WIDTH || newY < 0 || newY >= HEIGHT) return;
            TileType tile = map[newY][newX];
            if (tile != WALL && tile != DOOR_CLOSED) {
                if (p->isFire && tile == WATER) p->isDead = true;
                else if (!p->isFire && tile == LAVA) p->isDead = true;
                else if (tile == SPIKE) p->isDead = true;
                else {
                    p->x = newX; p->y = newY;
                    if (tile == ICE_FLOOR) { p->x += dx; p->y += dy; }
                    tryActivateSwitch(p->x, p->y);
                    tryTeleport(p->x, p->y);
                    if (tile == EXIT) {
                        Player* other = fireBoy.active ? &iceGirl : &fireBoy;
                        if (other->x == WIDTH - 2 && other->y == HEIGHT - 2) levelComplete = true;
                    }
                }
            }
        }
        if (fireBoy.isDead || iceGirl.isDead) gameOver = true;
    }

    // ================= 修改后的绘制逻辑 (双缓冲) =================
    void draw() {
        std::stringstream ss; // 帧缓冲区
        
        // 1. 光标复位，准备覆盖上一帧
        term.resetCursor();

        // 2. 绘制顶部
        ss << term.getColorStr(255, 255, 255);
        ss << "╔══════════════════════════════════════════════════════════════════╗\n";
        ss << "║  🎮 SSH Ice & Fire  │  Level: " << std::to_string(level).append(12 - std::to_string(level).length(), ' ') 
           << " │  Score: " << std::to_string(score).append(8 - std::to_string(score).length(), ' ') << "  ║\n";
        ss << "║  Tab: Switch  │  WASD/Arrows: Move  │  Q: Quit                    ║\n";
        ss << "╠══════════════════════════════════════════════════════════════════╣\n";
        ss << term.getResetStr();

        // 3. 绘制地图到流
        for (int y = 0; y < HEIGHT; y++) {
            ss << "║  ";
            for (int x = 0; x < WIDTH; x++) {
                if (fireBoy.x == x && fireBoy.y == y) {
                    if (fireBoy.active) ss << term.getBgColorStr(255, 100, 100) << term.getColorStr(255, 255, 255) << "●";
                    else ss << term.getColorStr(255, 100, 100) << "○";
                    ss << term.getResetStr();
                } else if (iceGirl.x == x && iceGirl.y == y) {
                    if (iceGirl.active) ss << term.getBgColorStr(100, 100, 255) << term.getColorStr(255, 255, 255) << "●";
                    else ss << term.getColorStr(100, 100, 255) << "○";
                    ss << term.getResetStr();
                } else {
                    TileType t = map[y][x];
                    switch (t) {
                        case WALL: ss << term.getColorStr(150, 150, 150) << "█"; break;
                        case WATER: ss << term.getColorStr(50, 150, 255) << "≈"; break;
                        case LAVA: ss << term.getColorStr(255, 100, 50) << "♨"; break;
                        case EXIT: ss << term.getColorStr(50, 255, 50) << "★"; break;
                        case SPIKE: ss << term.getColorStr(200, 200, 50) << "▲"; break;
                        case DOOR_CLOSED: ss << term.getColorStr(150, 100, 50) << "▓"; break;
                        case DOOR_OPEN: ss << term.getColorStr(100, 200, 100) << "░"; break;
                        case SWITCH: ss << term.getColorStr(255, 255, 100) << "◉"; break;
                        case TELEPORT_IN: ss << term.getColorStr(200, 100, 255) << "◐"; break;
                        case TELEPORT_OUT: ss << term.getColorStr(255, 100, 200) << "◑"; break;
                        case ICE_FLOOR: ss << term.getColorStr(150, 255, 255) << "·"; break;
                        case SAND_FLOOR: ss << term.getColorStr(200, 180, 100) << ","; break;
                        default: ss << " ";
                    }
                    ss << term.getResetStr();
                }
            }
            ss << "  ║\n";
        }
        
        // 4. 绘制底部图例
        ss << term.getColorStr(255, 255, 255);
        ss << "╠══════════════════════════════════════════════════════════════════╣\n";
        ss << "║  Legend: █=Wall ≈=Water ♨=Lava ▲=Spike ★=Exit ▓=Door ◉=Switch   ║\n";
        ss << "║          ◐=TeleIn ◑=TeleOut ·=Ice ,=Sand  ●=Active ○=Inactive   ║\n";
        ss << "╚══════════════════════════════════════════════════════════════════╝\n";
        ss << term.getResetStr();

        // 5. 最终一次性输出整个画面
        std::cout << ss.str() << std::flush;
    }

    void showLevelComplete() {
        term.clearScreen();
        std::cout << "\n\n" << term.getColorStr(50, 255, 50)
                  << "    ╔═══════════════════════════════════════╗\n"
                  << "    ║                                       ║\n"
                  << "    ║     🎉 LEVEL " << std::to_string(level).append(11 - std::to_string(level).length(), ' ') << " COMPLETE! 🎉          ║\n"
                  << "    ║                                       ║\n"
                  << "    ║     Score Bonus: +" << (level * 100) << " points                ║\n"
                  << "    ║                                       ║\n"
                  << "    ╚═══════════════════════════════════════╝\n" << term.getResetStr() << "\n";
        usleep(1500000);
        score += level * 100;
    }

    void showGameOver() {
        term.clearScreen();
        std::cout << "\n\n" << term.getColorStr(255, 50, 50)
                  << "    ╔═══════════════════════════════════════╗\n"
                  << "    ║                                       ║\n"
                  << "    ║         💀 GAME OVER 💀               ║\n"
                  << "    ║                                       ║\n"
                  << "    ║     " << (fireBoy.isDead ? "Fire Boy touched water!" : "Ice Girl touched lava/spike!") << "    ║\n"
                  << "    ║                                       ║\n"
                  << "    ║     Final Score: " << std::to_string(score).append(17 - std::to_string(score).length(), ' ') << "║\n"
                  << "    ║                                       ║\n"
                  << "    ╚═══════════════════════════════════════╝\n" << term.getResetStr() << "\n"
                  << "    Press 'r' to retry | Press 'q' to quit\n\n";
        term.showCursor();
        char c; read(STDIN_FILENO, &c, 1);
        term.hideCursor();
        if (c == 'r' || c == 'R') { score = 0; level = 1; generateLevel(); gameOver = false; run(); }
    }

    void run() {
        while (!gameOver) {
            processInput();
            if (levelComplete) {
                showLevelComplete();
                level++;
                if (level > MAX_LEVELS) { /* showVictory logic omitted for brevity */ return; }
                generateLevel();
            } else {
                draw();
                usleep(30000); // 略微提高到约 33 FPS，视觉更流畅
            }
        }
        showGameOver();
    }
};

int main() {
    std::cout << "\033[2J\033[H" << std::flush;
    std::cout << "\n\n    Loading SSH Ice & Fire (Double Buffered)...\n\n";
    usleep(500000);
    Game game;
    game.run();
    return 0;
}
