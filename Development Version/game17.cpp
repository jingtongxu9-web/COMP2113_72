#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <algorithm>
#include <sstream>
#include <random>
#include <chrono>

// ================= 配置 =================
const int WIDTH = 60;
const int HEIGHT = 25;
const int MAX_LEVELS = 20;

// ================= 排行榜动态内存结构 =================
struct PlayerRecord {
    std::string username;
    int score;
};

class RankingSystem {
private:
    PlayerRecord* records;
    int count;
    int capacity;

    void expandIfNeeded() {
        if (count < capacity) return;

        int newCapacity = (capacity == 0 ? 4 : capacity * 2);
        PlayerRecord* newRecords = new PlayerRecord[newCapacity];

        for (int i = 0; i < count; i++) {
            newRecords[i] = records[i];
        }

        delete[] records;
        records = newRecords;
        capacity = newCapacity;
    }

public:
    RankingSystem() : records(nullptr), count(0), capacity(0) {}

    ~RankingSystem() {
        delete[] records;
    }

    int findUser(const std::string& name) const {
        for (int i = 0; i < count; i++) {
            if (records[i].username == name) return i;
        }
        return -1;
    }

    int loginOrRegister(const std::string& name) {
        int idx = findUser(name);
        if (idx != -1) return idx;

        expandIfNeeded();
        records[count].username = name;
        records[count].score = 0;
        count++;
        return count - 1;
    }

    void addScore(int idx, int delta) {
        if (idx >= 0 && idx < count) {
            records[idx].score += delta;
        }
    }

    std::vector<PlayerRecord> getTopThree() const {
        std::vector<PlayerRecord> sorted;
        for (int i = 0; i < count; i++) {
            sorted.push_back(records[i]);
        }

        std::sort(sorted.begin(), sorted.end(), [](const PlayerRecord& a, const PlayerRecord& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.username < b.username;
        });

        if (sorted.size() > 3) sorted.resize(3);
        return sorted;
    }

    int getScore(int idx) const {
        if (idx >= 0 && idx < count) return records[idx].score;
        return 0;
    }

    std::string getUsername(int idx) const {
        if (idx >= 0 && idx < count) return records[idx].username;
        return "";
    }
};

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

    void resetCursor() {
        std::cout << "\033[H";
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
    SWITCH
};

enum SwitchState { OFF, ON };

struct Switch {
    int x, y;
    int linkedDoorX, linkedDoorY;
    SwitchState state;
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
    Player fireBoy, iceGirl;
    bool gameOver;
    bool levelComplete;
    bool gameCompleted; // NEW: only true when all levels are cleared
    unsigned int levelSeed;
    bool hasLevelSeed;
    int terrainLayers;
    int rewardPoints;

public:
    Game(int layers, int reward)
        : level(1), score(0), gameOver(false), levelComplete(false), gameCompleted(false),
          levelSeed(0), hasLevelSeed(false), terrainLayers(layers), rewardPoints(reward) {
        term.enableRawMode();
        term.hideCursor();
        generateLevel();
    }

    ~Game() {
        term.disableRawMode();
        term.showCursor();
        term.clearScreen();
    }

    int getAwardScore() const {
        return rewardPoints;
    }

    bool isGameCompleted() const {
        return gameCompleted;
    }

    void generateLevel(bool reuseSeed = false) {
        tcflush(STDIN_FILENO, TCIFLUSH);
        gameOver = false;
        levelComplete = false;
        switches.clear();
        map = std::vector<std::vector<TileType>>(HEIGHT, std::vector<TileType>(WIDTH, EMPTY));

        if (!reuseSeed || !hasLevelSeed) {
            levelSeed = static_cast<unsigned int>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()
            );
            hasLevelSeed = true;
        }

        std::mt19937 rng(levelSeed);
        auto R = [&](int n) {
            std::uniform_int_distribution<int> dist(0, n - 1);
            return dist(rng);
        };

        // 1. 生成外边界
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) {
                    map[y][x] = WALL;
                }
            }
        }

        // 2. 生成横贯屏障层（根据 terrainLayers 控制数量）
        std::vector<int> layerRows;
        if (terrainLayers == 3) {
            layerRows = {5, 13, 21};
        } else if (terrainLayers == 5) {
            layerRows = {5, 9, 13, 17, 21};
        } else { // 7
            layerRows = {3, 6, 9, 12, 15, 18, 21};
        }

        for (int y : layerRows) {
            bool coopLayer = (level >= 3 && R(100) < 50);

            if (coopLayer) {
                bool iceHelpsFire = (R(2) == 0);
                int doorX = iceHelpsFire ? (5 + R(15)) : (WIDTH - 20 + R(15));
                int swX = iceHelpsFire ? (WIDTH - 20 + R(15)) : (5 + R(15));
                int swY = y - 2;

                for (int x = 1; x < WIDTH - 1; x++) {
                    if (x == doorX) {
                        map[y][x] = DOOR_CLOSED;
                    } else {
                        if (iceHelpsFire) map[y][x] = (R(10) < 7 ? WATER : WALL);
                        else map[y][x] = (R(10) < 7 ? LAVA : WALL);
                    }
                }
                if (swY > 0 && swY < HEIGHT - 1) {
                    map[swY][swX] = SWITCH;
                    switches.push_back({swX, swY, doorX, y, OFF});
                }
            } else {
                int gap1 = R(WIDTH - 15) + 5;
                int gap2 = R(WIDTH - 15) + 10;
                for (int x = 1; x < WIDTH - 1; x++) {
                    if (x == gap1 || x == gap2) continue;
                    int r = R(100);
                    if (r < 40) map[y][x] = WALL;
                    else if (r < 60) map[y][x] = WATER;
                    else if (r < 80) map[y][x] = LAVA;
                    else map[y][x] = SPIKE;
                }
            }
        }

        // 3. 生成纵向障碍列
        int numVerticalCols = 2 + level / 3;
        for (int i = 0; i < numVerticalCols; i++) {
            int vx = R(WIDTH - 10) + 5;
            int startY = R(HEIGHT / 2);
            int len = R(10) + 5;
            int typeRoll = R(3);
            TileType vType = (typeRoll == 0 ? WALL : (typeRoll == 1 ? WATER : LAVA));

            for (int dy = 0; dy < len && (startY + dy) < HEIGHT - 1; dy++) {
                int cy = startY + dy;
                if (cy > 3 && cy < HEIGHT - 3 && map[cy][vx] != DOOR_CLOSED && map[cy][vx] != SWITCH) {
                    if (dy % 4 != 0) map[cy][vx] = vType;
                }
            }
        }

        // 4. 角色初始化
        fireBoy = {2, 2, 2, 2, true, true, false};
        iceGirl = {WIDTH - 3, 2, WIDTH - 3, 2, false, false, false};

        for (int dy = 0; dy < 3; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                map[1 + dy][1 + dx] = EMPTY;
                map[1 + dy][WIDTH - 2 - dx] = EMPTY;
            }
        }

        map[HEIGHT - 2][WIDTH / 2] = EXIT;
        map[HEIGHT - 3][WIDTH / 2] = EMPTY;
    }

    void toggleControl() {
        fireBoy.active = !fireBoy.active;
        iceGirl.active = !fireBoy.active;
    }

    void updateMechanics() {
        for (auto& sw : switches) {
            bool pressed = (fireBoy.x == sw.x && fireBoy.y == sw.y) ||
                           (iceGirl.x == sw.x && iceGirl.y == sw.y);

            if (pressed) {
                if (sw.state == OFF) score += 10;
                sw.state = ON;
                map[sw.linkedDoorY][sw.linkedDoorX] = DOOR_OPEN;
            } else {
                sw.state = OFF;
                map[sw.linkedDoorY][sw.linkedDoorX] = DOOR_CLOSED;

                if ((fireBoy.x == sw.linkedDoorX && fireBoy.y == sw.linkedDoorY) ||
                    (iceGirl.x == sw.linkedDoorX && iceGirl.y == sw.linkedDoorY)) {
                    fireBoy.isDead = true;
                    gameOver = true;
                }
            }
        }
    }

    void processInput() {
        char key = term.readKey();
        if (key == 0) return;
        if (key == 9) { toggleControl(); return; }
        if (key == 'r' || key == 'R') { generateLevel(false); return; }
        if (key == 'q' || key == 'Q') { gameOver = true; return; }

        Player* p = fireBoy.active ? &fireBoy : &iceGirl;
        p->prevX = p->x; p->prevY = p->y;
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
            if (newX < 0 || newX >= WIDTH || newY < 0 || newY >= HEIGHT) return;

            TileType tile = map[newY][newX];
            if (tile == WALL || tile == DOOR_CLOSED) return;

            p->x = newX; p->y = newY;

            if (p->isFire && map[p->y][p->x] == WATER) p->isDead = true;
            else if (!p->isFire && map[p->y][p->x] == LAVA) p->isDead = true;
            else if (map[p->y][p->x] == SPIKE) p->isDead = true;

            if (map[p->y][p->x] == EXIT) {
                Player* other = fireBoy.active ? &iceGirl : &fireBoy;
                if (other->x == WIDTH / 2 && other->y == HEIGHT - 2) levelComplete = true;
            }
        }
        if (fireBoy.isDead || iceGirl.isDead) gameOver = true;
    }

    void draw() {
        std::stringstream ss;
        ss << "\033[H";
        ss << term.getColorStr(255, 255, 255);
        ss << "╔══════════════════════════════════════════════════════════════════╗\n";
        ss << "║  🎮 SSH Ice & Fire  │  Level: " << std::to_string(level).append(12 - std::to_string(level).length(), ' ')
           << " │  Score: " << std::to_string(score).append(8 - std::to_string(score).length(), ' ') << "  ║\n";
        ss << "║  Tab: Switch  │  WASD/Arrows: Move  │  R: New Map │ Q: Quit      ║\n";
        ss << "╠══════════════════════════════════════════════════════════════════╣\n";
        ss << term.getResetStr();

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
                        case LAVA: ss << term.getColorStr(255, 100, 50) << "≈"; break;
                        case EXIT: ss << term.getColorStr(50, 255, 50) << "★"; break;
                        case SPIKE: ss << term.getColorStr(200, 200, 50) << "▲"; break;
                        case DOOR_CLOSED: ss << term.getColorStr(150, 100, 50) << "▓"; break;
                        case DOOR_OPEN: ss << term.getColorStr(100, 200, 100) << "░"; break;
                        case SWITCH: ss << term.getColorStr(255, 255, 100) << "◉"; break;
                        default: ss << " ";
                    }
                    ss << term.getResetStr();
                }
            }
            ss << "  ║\n";
        }

        ss << term.getColorStr(255, 255, 255);
        ss << "╠══════════════════════════════════════════════════════════════════╣\n";
        ss << "║  Legend: █=Wall ≈=Water ≈=Lava ▲=Spike ★=Exit ▓=Door ◉=Switch   ║\n";
        ss << "║                                         ●=Active ○=Inactive      ║\n";
        ss << "╚══════════════════════════════════════════════════════════════════╝\n";
        ss << term.getResetStr();

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
        term.clearScreen();
    }

    void showGameOver() {
        term.clearScreen();
        std::cout << "\n\n" << term.getColorStr(255, 50, 50)
                  << "    ╔═══════════════════════════════════════╗\n"
                  << "    ║                                       ║\n"
                  << "    ║         💀 GAME OVER 💀               ║\n"
                  << "    ║                                       ║\n"
                  << "    ║  Character was trapped or hit hazard  ║\n"
                  << "    ║                                       ║\n"
                  << "    ║     Final Score: " << std::to_string(score).append(17 - std::to_string(score).length(), ' ') << "║\n"
                  << "    ║                                       ║\n"
                  << "    ╚═══════════════════════════════════════╝\n" << term.getResetStr() << "\n"
                  << "    Press 'm' to replay same map | Press 'r' to restart | Press 'q' to quit\n\n";

        term.showCursor();
        char c;
        read(STDIN_FILENO, &c, 1);
        term.hideCursor();

        if (c == 'm' || c == 'M') {
            fireBoy.isDead = false;
            iceGirl.isDead = false;
            gameOver = false;
            generateLevel(true);
            run();
        } else if (c == 'r' || c == 'R') {
            score = 0;
            level = 1;
            fireBoy.isDead = false;
            iceGirl.isDead = false;
            gameOver = false;
            generateLevel(false);
            run();
        }
    }

    void run() {
        while (!gameOver) {
            processInput();
            updateMechanics();
            if (levelComplete) {
                showLevelComplete();
                level++;
                if (level > MAX_LEVELS) {
                    gameCompleted = true; // NEW: award only when fully completed
                    return;
                }
                generateLevel(false);
            } else {
                draw();
                usleep(25000);
            }
        }
        showGameOver();
    }
};

void showMainMenu(const RankingSystem& ranking) {
    std::cout << "\033[2J\033[H" << std::flush;
    std::cout
        << "============================================================\n"
        << "                      SSH ICE & FIRE                        \n"
        << "============================================================\n\n"
        << "Select difficulty level:\n"
        << "  1) Level 1 (3 terrain layers, +10 points)\n"
        << "  2) Level 2 (5 terrain layers, +30 points)\n"
        << "  3) Level 3 (7 terrain layers, +50 points)\n\n"
        << "Top 3 Players:\n";

    std::vector<PlayerRecord> top = ranking.getTopThree();
    if (top.empty()) {
        std::cout << "  No records yet.\n";
    } else {
        for (size_t i = 0; i < top.size(); i++) {
            std::cout << "  " << (i + 1) << ". " << top[i].username << " - " << top[i].score << "\n";
        }
    }

    std::cout << "\n";
}

std::string inputUsername() {
    std::string username;
    std::cout << "Enter your username: ";
    std::cin >> username;
    return username;
}

int inputDifficulty() {
    while (true) {
        std::cout << "Enter 1, 2, or 3 to start: ";
        char c;
        std::cin >> c;

        if (c == '1') return 1;
        if (c == '2') return 2;
        if (c == '3') return 3;

        std::cout << "Invalid input. Please enter 1, 2, or 3.\n";
    }
}

int main() {
    RankingSystem ranking;

    while (true) {
        showMainMenu(ranking);

        std::cout << "Please input your username.\n";
        std::cout << "Use this same username in future logins to keep your score record.\n\n";

        std::string username = inputUsername();
        int userIndex = ranking.loginOrRegister(username);

        std::cout << "\nWelcome, " << ranking.getUsername(userIndex)
                  << ". Current total score: " << ranking.getScore(userIndex) << "\n\n";

        int difficulty = inputDifficulty();

        int layers = 5;
        int reward = 30;
        if (difficulty == 1) {
            layers = 3;
            reward = 10;
        } else if (difficulty == 2) {
            layers = 5;
            reward = 30;
        } else {
            layers = 7;
            reward = 50;
        }

        Game game(layers, reward);
        game.run();

        if (game.isGameCompleted()) {
            ranking.addScore(userIndex, reward);
        }

        std::cout << "\033[2J\033[H" << std::flush;
        std::cout << "Game finished.\n";
        std::cout << "Player: " << ranking.getUsername(userIndex) << "\n";
        if (game.isGameCompleted()) {
            std::cout << "Added score: +" << reward << "\n";
        } else {
            std::cout << "Added score: +0 (game not completed)\n";
        }
        std::cout << "New total score: " << ranking.getScore(userIndex) << "\n\n";
        std::cout << "Press y to return to main menu, or any other key to quit: ";

        char again;
        std::cin >> again;
        if (again != 'y' && again != 'Y') break;
    }

    return 0;
}
