#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <algorithm>

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

    void setBgColor(int r, int g, int b) {
        std::cout << "\033[48;2;" << r << ";" << g << ";" << b << "m";
    }
    
    void resetColor() {
        std::cout << "\033[0m";
    }

    void moveCursor(int x, int y) {
        std::cout << "\033[" << y << ";" << x << "H" << std::flush;
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
        term.clear();
    }

    // 生成关卡
    void generateLevel() {
        gameOver = false;
        win = false;
        levelComplete = false;
        switches.clear();
        teleports.clear();
        
        // 初始化地图
        map = std::vector<std::vector<TileType>>(HEIGHT, std::vector<TileType>(WIDTH, EMPTY));
        
        // 1. 生成边界墙
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) {
                    map[y][x] = WALL;
                }
            }
        }

        // 2. 生成内部结构 (随层数增加复杂度)
        int complexity = level * 3;
        int obstacleDensity = 5 + level;
        
        // 生成平台
        for (int i = 0; i < complexity + 5; i++) {
            int platY = rand() % (HEIGHT - 4) + 2;
            int platX = rand() % (WIDTH - 10) + 3;
            int platLen = rand() % 8 + 3;
            
            for (int j = 0; j < platLen && platX + j < WIDTH - 1; j++) {
                if (map[platY][platX + j] == EMPTY) {
                    map[platY][platX + j] = WALL;
                }
            }
        }

        // 3. 生成障碍 (多样性)
        for (int y = 2; y < HEIGHT - 2; y++) {
            for (int x = 2; x < WIDTH - 2; x++) {
                if (map[y][x] == EMPTY && rand() % 100 < obstacleDensity) {
                    int obstacleType = rand() % 100;
                    
                    if (obstacleType < 30) {
                        map[y][x] = WATER;
                    } else if (obstacleType < 55) {
                        map[y][x] = LAVA;
                    } else if (obstacleType < 70 && level >= 2) {
                        map[y][x] = SPIKE;
                    } else if (obstacleType < 80 && level >= 3) {
                        map[y][x] = ICE_FLOOR;
                    } else if (obstacleType < 90 && level >= 4) {
                        map[y][x] = SAND_FLOOR;
                    }
                }
            }
        }

        // 4. 生成开关和门 (层数>=2)
        if (level >= 2) {
            int numSwitches = std::min(level / 2, 3);
            for (int i = 0; i < numSwitches; i++) {
                int sx, sy, dx, dy;
                // 找合适位置放开关
                do {
                    sx = rand() % (WIDTH - 4) + 2;
                    sy = rand() % (HEIGHT - 4) + 2;
                } while (map[sy][sx] != EMPTY);
                
                // 找合适位置放门
                do {
                    dx = rand() % (WIDTH - 4) + 2;
                    dy = rand() % (HEIGHT - 4) + 2;
                } while (map[dy][dx] != WALL);
                
                switches.push_back({sx, sy, dx, dy, OFF});
                map[sy][sx] = SWITCH;
                map[dy][dx] = DOOR_CLOSED;
            }
        }

        // 5. 生成传送点 (层数>=5)
        if (level >= 5) {
            int tx1 = rand() % (WIDTH - 4) + 2;
            int ty1 = rand() % (HEIGHT - 4) + 2;
            int tx2 = rand() % (WIDTH - 4) + 2;
            int ty2 = rand() % (HEIGHT - 4) + 2;
            
            if (map[ty1][tx1] == EMPTY && map[ty2][tx2] == EMPTY) {
                map[ty1][tx1] = TELEPORT_IN;
                map[ty2][tx2] = TELEPORT_OUT;
                teleports.push_back({tx1, ty1, tx2, ty2});
            }
        }

        // 6. 放置玩家 (确保出生点安全)
        fireBoy = {2, 2, 2, 2, true, true, false};
        iceGirl = {WIDTH - 3, 2, WIDTH - 3, 2, false, false, false};
        
        // 清理出生点周围
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = fireBoy.x + dx, ny = fireBoy.y + dy;
                if (nx > 0 && nx < WIDTH-1 && ny > 0 && ny < HEIGHT-1) {
                    if (map[ny][nx] == WATER || map[ny][nx] == LAVA || map[ny][nx] == SPIKE) {
                        map[ny][nx] = EMPTY;
                    }
                }
                nx = iceGirl.x + dx; ny = iceGirl.y + dy;
                if (nx > 0 && nx < WIDTH-1 && ny > 0 && ny < HEIGHT-1) {
                    if (map[ny][nx] == WATER || map[ny][nx] == LAVA || map[ny][nx] == SPIKE) {
                        map[ny][nx] = EMPTY;
                    }
                }
            }
        }

        // 7. 放置出口 (右下角，确保可达)
        map[HEIGHT - 2][WIDTH - 2] = EXIT;
        for (int dy = -2; dy <= 0; dy++) {
            for (int dx = -2; dx <= 0; dx++) {
                int nx = WIDTH - 2 + dx, ny = HEIGHT - 2 + dy;
                if (nx > 0 && nx < WIDTH-1 && ny > 0 && ny < HEIGHT-1) {
                    if (map[ny][nx] == WATER || map[ny][nx] == LAVA || map[ny][nx] == SPIKE) {
                        map[ny][nx] = EMPTY;
                    }
                }
            }
        }
    }

    // 切换控制角色
    void toggleControl() {
        fireBoy.active = !fireBoy.active;
        iceGirl.active = !fireBoy.active;
    }

    // 踩开关
    void tryActivateSwitch(int x, int y) {
        for (auto& sw : switches) {
            if (sw.x == x && sw.y == y && sw.state == OFF) {
                sw.state = ON;
                // 打开对应的门
                map[sw.linkedDoorY][sw.linkedDoorX] = DOOR_OPEN;
                score += 50;
            }
        }
    }

    // 传送
    void tryTeleport(int& x, int& y) {
        for (auto& tp : teleports) {
            if (tp.inX == x && tp.inY == y) {
                x = tp.outX;
                y = tp.outY;
                score += 25;
                break;
            }
        }
    }

    // 处理输入
    void processInput() {
        char key = term.readKey();
        if (key == 0) return;

        if (key == 9) { // Tab
            toggleControl();
            return;
        }
        if (key == 'q' || key == 'Q') { 
            gameOver = true; 
            win = false; 
            return; 
        }

        Player* p = fireBoy.active ? &fireBoy : &iceGirl;
        p->prevX = p->x;
        p->prevY = p->y;
        int dx = 0, dy = 0;

        if (key == 27) { 
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[1] == 'A') dy = -1;
                else if (seq[1] == 'B') dy = 1;
                else if (seq[1] == 'C') dx = 1;
                else if (seq[1] == 'D') dx = -1;
            }
        } else {
            if (key == 'w' || key == 'W') dy = -1;
            else if (key == 's' || key == 'S') dy = 1;
            else if (key == 'd' || key == 'D') dx = 1;
            else if (key == 'a' || key == 'A') dx = -1;
        }

        if (dx != 0 || dy != 0) {
            int newX = p->x + dx;
            int newY = p->y + dy;
            TileType tile = map[newY][newX];

            // 墙壁和关闭的门不能通过
            if (tile != WALL && tile != DOOR_CLOSED) {
                // 属性检测
                if (p->isFire && tile == WATER) { p->isDead = true; }
                else if (!p->isFire && tile == LAVA) { p->isDead = true; }
                else if (tile == SPIKE) { p->isDead = true; }
                else {
                    p->x = newX;
                    p->y = newY;
                    
                    // 特殊地板效果
                    if (tile == ICE_FLOOR) {
                        // 冰面滑行 (继续移动一格)
                        p->x += dx;
                        p->y += dy;
                    } else if (tile == SAND_FLOOR) {
                        // 沙地减速 (已简化为正常移动)
                    }
                    
                    // 踩开关
                    tryActivateSwitch(p->x, p->y);
                    
                    // 传送
                    tryTeleport(p->x, p->y);
                    
                    // 胜利检测
                    if (tile == EXIT) {
                        Player* other = fireBoy.active ? &iceGirl : &fireBoy;
                        if (other->x == WIDTH - 2 && other->y == HEIGHT - 2) {
                            levelComplete = true;
                        }
                    }
                }
            }
        }
        
        // 检查死亡
        if (fireBoy.isDead || iceGirl.isDead) {
            gameOver = true;
        }
    }

    // 下落过渡动画
    void fallAnimation() {
        term.clear();
        std::cout << "\n\n";
        
        for (int frame = 0; frame < ANIMATION_FRAMES; frame++) {
            term.clear();
            
            // 绘制下落效果
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    if (y < frame * 2) {
                        term.setColor(100, 100, 255);
                        std::cout << "|";
                    } else if (y == frame * 2) {
                        term.setColor(255, 255, 100);
                        std::cout << "*";
                    } else {
                        std::cout << " ";
                    }
                }
                std::cout << "\n";
            }
            
            std::cout << "\n    Falling to Level " << (level + 1) << "...\n";
            term.resetColor();
            usleep(80000);
        }
        
        term.clear();
    }

    // 绘制游戏画面
    void draw() {
        term.clear();
        
        // 顶部信息栏
        term.setColor(255, 255, 255);
        std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  🎮 SSH Ice & Fire  │  Level: " << std::to_string(level).append(12 - std::to_string(level).length(), ' ') 
                  << " │  Score: " << std::to_string(score).append(8 - std::to_string(score).length(), ' ') << "  ║\n";
        std::cout << "║  Tab: Switch  │  WASD/Arrows: Move  │  Q: Quit                    ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
        term.resetColor();

        // 游戏地图
        for (int y = 0; y < HEIGHT; y++) {
            std::cout << "║  ";
            for (int x = 0; x < WIDTH; x++) {
                bool isFirePos = (fireBoy.x == x && fireBoy.y == y);
                bool isIcePos = (iceGirl.x == x && iceGirl.y == y);

                if (isFirePos) {
                    if (fireBoy.active) {
                        term.setBgColor(255, 100, 100);
                        term.setColor(255, 255, 255);
                        std::cout << "●";
                    } else {
                        term.setColor(255, 100, 100);
                        std::cout << "○";
                    }
                    term.resetColor();
                } else if (isIcePos) {
                    if (iceGirl.active) {
                        term.setBgColor(100, 100, 255);
                        term.setColor(255, 255, 255);
                        std::cout << "●";
                    } else {
                        term.setColor(100, 100, 255);
                        std::cout << "○";
                    }
                    term.resetColor();
                } else {
                    TileType t = map[y][x];
                    switch (t) {
                        case WALL:
                            term.setColor(150, 150, 150);
                            std::cout << "█";
                            break;
                        case WATER:
                            term.setColor(50, 150, 255);
                            std::cout << "≈";
                            break;
                        case LAVA:
                            term.setColor(255, 100, 50);
                            std::cout << "♨";
                            break;
                        case EXIT:
                            term.setColor(50, 255, 50);
                            std::cout << "★";
                            break;
                        case SPIKE:
                            term.setColor(200, 200, 50);
                            std::cout << "▲";
                            break;
                        case DOOR_CLOSED:
                            term.setColor(150, 100, 50);
                            std::cout << "▓";
                            break;
                        case DOOR_OPEN:
                            term.setColor(100, 200, 100);
                            std::cout << "░";
                            break;
                        case SWITCH:
                            term.setColor(255, 255, 100);
                            std::cout << "◉";
                            break;
                        case TELEPORT_IN:
                            term.setColor(200, 100, 255);
                            std::cout << "◐";
                            break;
                        case TELEPORT_OUT:
                            term.setColor(255, 100, 200);
                            std::cout << "◑";
                            break;
                        case ICE_FLOOR:
                            term.setColor(150, 255, 255);
                            std::cout << "·";
                            break;
                        case SAND_FLOOR:
                            term.setColor(200, 180, 100);
                            std::cout << ",";
                            break;
                        default:
                            std::cout << " ";
                    }
                    term.resetColor();
                }
            }
            std::cout << "  ║\n";
        }
        
        // 底部图例
        term.setColor(255, 255, 255);
        std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Legend: █=Wall ≈=Water ♨=Lava ▲=Spike ★=Exit ▓=Door ◉=Switch   ║\n";
        std::cout << "║          ◐=TeleIn ◑=TeleOut ·=Ice ,=Sand  ●=Active ○=Inactive   ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
        term.resetColor();
    }

    // 显示关卡完成
    void showLevelComplete() {
        term.clear();
        std::cout << "\n\n";
        term.setColor(50, 255, 50);
        std::cout << "    ╔═══════════════════════════════════════╗\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ║     🎉 LEVEL " << std::to_string(level).append(11 - std::to_string(level).length(), ' ') << " COMPLETE! 🎉          ║\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ║     Score Bonus: +" << (level * 100) << " points                ║\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ╚═══════════════════════════════════════╝\n";
        term.resetColor();
        std::cout << "\n";
        usleep(1500000);
        
        score += level * 100;
    }

    // 显示游戏结束
    void showGameOver() {
        term.clear();
        std::cout << "\n\n";
        term.setColor(255, 50, 50);
        std::cout << "    ╔═══════════════════════════════════════╗\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ║         💀 GAME OVER 💀               ║\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ║     " << (fireBoy.isDead ? "Fire Boy touched water!" : "Ice Girl touched lava/spike!") << "    ║\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ║     Final Score: " << std::to_string(score).append(17 - std::to_string(score).length(), ' ') << "║\n";
        std::cout << "    ║                                       ║\n";
        std::cout << "    ╚═══════════════════════════════════════╝\n";
        term.resetColor();
        std::cout << "\n";
        std::cout << "    Press 'r' to retry | Press 'q' to quit\n\n";
        
        term.showCursor();
        char c;
        read(STDIN_FILENO, &c, 1);
        term.hideCursor();
        
        if (c == 'r' || c == 'R') {
            score = 0;
            level = 1;
            generateLevel();
            gameOver = false;
        }
    }

    // 显示通关
    void showVictory() {
        term.clear();
        std::cout << "\n\n";
        term.setColor(255, 215, 0);
        std::cout << "    ╔═══════════════════════════════════════════════════╗\n";
        std::cout << "    ║                                                   ║\n";
        std::cout << "    ║           🏆 CONGRATULATIONS! 🏆                  ║\n";
        std::cout << "    ║                                                   ║\n";
        std::cout << "    ║      You have completed all " << MAX_LEVELS << " levels!            ║\n";
        std::cout << "    ║                                                   ║\n";
        std::cout << "    ║      Final Score: " << std::to_string(score).append(29 - std::to_string(score).length(), ' ') << "║\n";
        std::cout << "    ║                                                   ║\n";
        std::cout << "    ║      You are a true Ice & Fire Master!           ║\n";
        std::cout << "    ║                                                   ║\n";
        std::cout << "    ╚═══════════════════════════════════════════════════╝\n";
        term.resetColor();
        std::cout << "\n";
        std::cout << "    Press any key to exit...\n";
        
        term.showCursor();
        char c;
        read(STDIN_FILENO, &c, 1);
    }

    // 主循环
    void run() {
        while (!gameOver) {
            processInput();
            
            if (levelComplete) {
                showLevelComplete();
                fallAnimation();
                level++;
                if (level > MAX_LEVELS) {
                    showVictory();
                    return;
                }
                generateLevel();
                levelComplete = false;
            } else {
                draw();
                usleep(50000);
            }
        }
        
        showGameOver();
        
        if (!gameOver) {
            run();
        }
    }
};

int main() {
    std::cout << "\033[2J\033[H" << std::flush;
    std::cout << "\n\n    Loading SSH Ice & Fire...\n\n";
    usleep(500000);
    
    Game game;
    game.run();
    
    return 0;
}


