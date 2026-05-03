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
#include <fstream>
#include <limits>

// ================= 配置 =================
const int WIDTH = 60;
const int HEIGHT = 25;
const int MAX_LEVELS = 20;
const char* RANKING_FILE = "ranking_data.txt";
const char* FIXED_PROGRESS_FILE = "fixed_progress_data.txt";
const char* RANDOM_LEVEL_RECORD_FILE = "random_level_records.txt";
const int FALL_INTERVAL_US = 200000;  // falling speed: one row every 0.20 second

// ================= 固定关卡配置 =================
// Fixed Level Mode uses deterministic seeds. The original procedural generation
// logic is still used, but the seed, level number, and terrain layer count are
// fixed, so each selected level always produces the same map.
const int FIXED_LEVEL_COUNT = 10;
const unsigned int FIXED_LEVEL_SEEDS[FIXED_LEVEL_COUNT + 1] = {
    0,
    12031, 24067, 36109,      // Difficulty 1: fixed levels 1-3
    48221, 59333, 71441,      // Difficulty 2: fixed levels 4-6
    83563, 95617, 107741, 118873  // Difficulty 3: fixed levels 7-10
};

struct FixedLevelConfig {
    int fixedLevelId;
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    unsigned int seed;
};

FixedLevelConfig getFixedLevelConfig(int fixedLevelId) {
    if (fixedLevelId < 1) fixedLevelId = 1;
    if (fixedLevelId > FIXED_LEVEL_COUNT) fixedLevelId = FIXED_LEVEL_COUNT;

    FixedLevelConfig cfg;
    cfg.fixedLevelId = fixedLevelId;
    cfg.seed = FIXED_LEVEL_SEEDS[fixedLevelId];

    if (fixedLevelId <= 3) {
        cfg.difficulty = 1;
        cfg.terrainLayers = 3;
        cfg.rewardPoints = 10;
    } else if (fixedLevelId <= 6) {
        cfg.difficulty = 2;
        cfg.terrainLayers = 5;
        cfg.rewardPoints = 30;
    } else {
        cfg.difficulty = 3;
        cfg.terrainLayers = 7;
        cfg.rewardPoints = 50;
    }

    return cfg;
}

struct DifficultyConfig {
    int difficulty;
    int terrainLayers;
    int rewardPoints;
};

DifficultyConfig getDifficultyConfig(int difficulty) {
    if (difficulty < 1) difficulty = 1;
    if (difficulty > 3) difficulty = 3;

    DifficultyConfig cfg;
    cfg.difficulty = difficulty;

    if (difficulty == 1) {
        cfg.terrainLayers = 3;
        cfg.rewardPoints = 10;
    } else if (difficulty == 2) {
        cfg.terrainLayers = 5;
        cfg.rewardPoints = 30;
    } else {
        cfg.terrainLayers = 7;
        cfg.rewardPoints = 50;
    }

    return cfg;
}


// ================= 固定关卡进度与随机关卡记录 =================
struct FixedProgressRecord {
    std::string username;
    int lastFixedLevel;
};

class FixedProgressSystem {
private:
    std::vector<FixedProgressRecord> records;

    int findUser(const std::string& username) const {
        for (size_t i = 0; i < records.size(); i++) {
            if (records[i].username == username) return static_cast<int>(i);
        }
        return -1;
    }

public:
    int getLastFixedLevel(const std::string& username) const {
        int idx = findUser(username);
        if (idx == -1) return 1;
        if (records[idx].lastFixedLevel < 1) return 1;
        if (records[idx].lastFixedLevel > FIXED_LEVEL_COUNT) return FIXED_LEVEL_COUNT;
        return records[idx].lastFixedLevel;
    }

    void setLastFixedLevel(const std::string& username, int levelId) {
        if (levelId < 1) levelId = 1;
        if (levelId > FIXED_LEVEL_COUNT) levelId = FIXED_LEVEL_COUNT;

        int idx = findUser(username);
        if (idx == -1) {
            records.push_back({username, levelId});
        } else {
            records[idx].lastFixedLevel = levelId;
        }
    }

    bool loadFromFile(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) return false;

        int loadedCount = 0;
        in >> loadedCount;
        if (!in.good() || loadedCount < 0) return false;

        records.clear();
        for (int i = 0; i < loadedCount; i++) {
            std::string username;
            int levelId = 1;
            if (!(in >> username >> levelId)) break;
            if (!username.empty()) setLastFixedLevel(username, levelId);
        }
        return true;
    }

    bool saveToFile(const std::string& filename) const {
        std::ofstream out(filename, std::ios::trunc);
        if (!out.is_open()) return false;

        out << records.size() << "\n";
        for (const auto& record : records) {
            out << record.username << " " << record.lastFixedLevel << "\n";
        }
        return true;
    }
};

struct RandomLevelResult {
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    int levelNumber;
    unsigned int seed;
    int scoreEarned;
    bool completed;
};

struct RandomLevelRecord {
    int recordId;
    std::string username;
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    int levelNumber;
    unsigned int seed;
    int scoreEarned;
    bool completed;
};

struct RecordedRandomConfig {
    int recordId;
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    int levelNumber;
    unsigned int seed;
};

class RandomLevelArchive {
private:
    std::vector<RandomLevelRecord> records;
    int nextRecordId;

public:
    RandomLevelArchive() : nextRecordId(1) {}

    bool loadFromFile(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) return false;

        int loadedCount = 0;
        in >> loadedCount;
        if (!in.good() || loadedCount < 0) return false;

        records.clear();
        nextRecordId = 1;

        for (int i = 0; i < loadedCount; i++) {
            RandomLevelRecord record;
            int completedInt = 0;
            if (!(in >> record.recordId >> record.username >> record.difficulty
                     >> record.terrainLayers >> record.rewardPoints >> record.levelNumber
                     >> record.seed >> record.scoreEarned >> completedInt)) {
                break;
            }

            record.completed = (completedInt != 0);
            records.push_back(record);
            if (record.recordId >= nextRecordId) nextRecordId = record.recordId + 1;
        }

        return true;
    }

    bool saveToFile(const std::string& filename) const {
        std::ofstream out(filename, std::ios::trunc);
        if (!out.is_open()) return false;

        out << records.size() << "\n";
        for (const auto& record : records) {
            out << record.recordId << " "
                << record.username << " "
                << record.difficulty << " "
                << record.terrainLayers << " "
                << record.rewardPoints << " "
                << record.levelNumber << " "
                << record.seed << " "
                << record.scoreEarned << " "
                << (record.completed ? 1 : 0) << "\n";
        }
        return true;
    }

    void addRecord(const std::string& username, const RandomLevelResult& result) {
        RandomLevelRecord record;
        record.recordId = nextRecordId++;
        record.username = username;
        record.difficulty = result.difficulty;
        record.terrainLayers = result.terrainLayers;
        record.rewardPoints = result.rewardPoints;
        record.levelNumber = result.levelNumber;
        record.seed = result.seed;
        record.scoreEarned = result.scoreEarned;
        record.completed = result.completed;
        records.push_back(record);
    }

    std::vector<RandomLevelRecord> getUserRecords(const std::string& username) const {
        std::vector<RandomLevelRecord> userRecords;
        for (const auto& record : records) {
            if (record.username == username) userRecords.push_back(record);
        }

        std::sort(userRecords.begin(), userRecords.end(), [](const RandomLevelRecord& a, const RandomLevelRecord& b) {
            return a.recordId > b.recordId;
        });

        return userRecords;
    }

    bool findRecordByIdForUser(const std::string& username, int recordId, RandomLevelRecord& outRecord) const {
        for (const auto& record : records) {
            if (record.username == username && record.recordId == recordId) {
                outRecord = record;
                return true;
            }
        }
        return false;
    }

    bool findNewestRecordBySeedForUser(const std::string& username, unsigned int seed, RandomLevelRecord& outRecord) const {
        bool found = false;
        int newestRecordId = -1;

        for (const auto& record : records) {
            if (record.username == username && record.seed == seed && record.recordId > newestRecordId) {
                outRecord = record;
                newestRecordId = record.recordId;
                found = true;
            }
        }

        return found;
    }
};


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
        for (int i = 0; i < count; i++) sorted.push_back(records[i]);

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

    bool loadFromFile(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) return false;

        int loadedCount = 0;
        in >> loadedCount;
        if (!in.good() || loadedCount < 0) return false;

        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        delete[] records;
        records = nullptr;
        count = 0;
        capacity = 0;

        for (int i = 0; i < loadedCount; i++) {
            std::string line;
            if (!std::getline(in, line)) break;

            size_t tabPos = line.find('\t');
            if (tabPos == std::string::npos) continue;

            std::string name = line.substr(0, tabPos);
            std::string scoreStr = line.substr(tabPos + 1);

            int sc = 0;
            try {
                sc = std::stoi(scoreStr);
            } catch (...) {
                continue;
            }

            if (name.empty()) continue;

            expandIfNeeded();
            records[count].username = name;
            records[count].score = sc;
            count++;
        }

        return true;
    }

    bool saveToFile(const std::string& filename) const {
        std::ofstream out(filename, std::ios::trunc);
        if (!out.is_open()) return false;

        out << count << "\n";
        for (int i = 0; i < count; i++) {
            out << records[i].username << "\t" << records[i].score << "\n";
        }
        return true;
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
    bool gameCompleted;
    unsigned int levelSeed;
    bool hasLevelSeed;
    int terrainLayers;
    int rewardPoints;
    int difficultyId;
    bool fixedLevelMode;
    int fixedLevelId;
    bool recordedRandomMode;
    int recordedRecordId;
    unsigned int recordedRandomSeed;
    bool chooseOtherFixedLevel;
    bool chooseOtherRecordedLevel;
    int levelStartScore;
    std::vector<RandomLevelResult> randomLevelResults;

    bool isBlockingTile(TileType tile) const {
        return tile == WALL || tile == DOOR_CLOSED;
    }

    bool isHazardForPlayer(const Player& p, TileType tile) const {
        if (tile == SPIKE) return true;
        if (p.isFire && tile == WATER) return true;
        if (!p.isFire && tile == LAVA) return true;
        return false;
    }

    void checkCurrentTile(Player& p) {
        TileType tile = map[p.y][p.x];
        if (isHazardForPlayer(p, tile)) {
            p.isDead = true;
            gameOver = true;
        }
        if (tile == EXIT) {
            Player* other = (&p == &fireBoy) ? &iceGirl : &fireBoy;
            if (other->x == WIDTH / 2 && other->y == HEIGHT - 2) levelComplete = true;
        }
    }

    bool applyGravityToPlayer(Player& p) {
        if (gameOver || levelComplete || p.isDead) return false;
        if (p.y + 1 >= HEIGHT) return false;

        TileType below = map[p.y + 1][p.x];
        if (isBlockingTile(below)) return false;

        if (isHazardForPlayer(p, below)) {
            p.prevX = p.x;
            p.prevY = p.y;
            p.y++;
            p.isDead = true;
            gameOver = true;
            return true;
        }

        p.prevX = p.x;
        p.prevY = p.y;
        p.y++;
        checkCurrentTile(p);
        return true;
    }

    bool applyGravity() {
        bool changed = false;
        changed = applyGravityToPlayer(fireBoy) || changed;
        changed = applyGravityToPlayer(iceGirl) || changed;
        return changed;
    }

    bool tryMovePlayer(Player& p, int dx, int dy) {
        int newX = p.x + dx;
        int newY = p.y + dy;
        if (newX < 0 || newX >= WIDTH || newY < 0 || newY >= HEIGHT) return false;

        TileType tile = map[newY][newX];
        if (isBlockingTile(tile)) return false;

        p.prevX = p.x;
        p.prevY = p.y;
        p.x = newX;
        p.y = newY;

        checkCurrentTile(p);
        return true;
    }


    bool canOverwriteForFallSafety(TileType tile) const {
        return tile == EMPTY || tile == WATER || tile == LAVA || tile == SPIKE || tile == WALL;
    }

    void clearFairFallSpaceAroundOpening(int openingX, int layerY) {
        // A fall starts when a character steps from the platform into an empty opening.
        // The first few rows under that opening must stay free of instant hazards,
        // otherwise the player cannot react in time while falling.
        const int REACTION_ROWS = 4;
        const int REACTION_WIDTH = 2;

        for (int y = layerY + 1; y <= layerY + REACTION_ROWS && y < HEIGHT - 1; y++) {
            for (int x = openingX - REACTION_WIDTH; x <= openingX + REACTION_WIDTH; x++) {
                if (x <= 0 || x >= WIDTH - 1) continue;
                if (map[y][x] == DOOR_CLOSED || map[y][x] == SWITCH || map[y][x] == EXIT) continue;
                if (canOverwriteForFallSafety(map[y][x])) map[y][x] = EMPTY;
            }
        }
    }

    void makeFallRoutesFair(const std::vector<int>& layerRows) {
        // For every opening in a platform layer, create a small empty "reaction zone" below it.
        // This prevents impossible cases where a lava/water/spike tile appears only one row
        // below and one column away from the start of the fall.
        for (int layerY : layerRows) {
            for (int x = 1; x < WIDTH - 1; x++) {
                if (map[layerY][x] == EMPTY) {
                    clearFairFallSpaceAroundOpening(x, layerY);
                }
            }
        }

        // Also protect the two starting platforms. If a player walks off the left/right edge,
        // the first falling space should not immediately contain hazards.
        for (int x = 1; x <= 3; x++) clearFairFallSpaceAroundOpening(x, 4);
        for (int x = WIDTH - 4; x <= WIDTH - 2; x++) clearFairFallSpaceAroundOpening(x, 4);
    }

    void clearSmallArea(int cx, int cy, int radiusX, int radiusY) {
        for (int y = cy - radiusY; y <= cy + radiusY; y++) {
            for (int x = cx - radiusX; x <= cx + radiusX; x++) {
                if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= HEIGHT - 1) continue;
                if (map[y][x] == EXIT) continue;
                map[y][x] = EMPTY;
            }
        }
    }

    void repairDoorAccess(const std::vector<int>& layerRows) {
        // A closed door is usually placed inside a platform row. In a gravity-based map,
        // the door becomes unfair or even unreachable if a wall/hazard is packed directly
        // above it. Keep only a local access shaft, so the rest of the map is still complex.
        for (auto& sw : switches) {
            int doorX = sw.linkedDoorX;
            int doorY = sw.linkedDoorY;

            // Find the nearest platform layer above this door. The falling character needs
            // a short controllable shaft from that upper layer toward the door area.
            int upperLayer = -1;
            for (int y : layerRows) {
                if (y < doorY && y > upperLayer) upperLayer = y;
            }

            if (upperLayer != -1) {
                int left = std::max(1, doorX - 2);
                int right = std::min(WIDTH - 2, doorX + 2);
                for (int y = upperLayer + 1; y < doorY; y++) {
                    for (int x = left; x <= right; x++) {
                        if (map[y][x] != SWITCH && map[y][x] != EXIT) map[y][x] = EMPTY;
                    }
                }

                // Make sure there is at least one opening in the upper platform near the door.
                // This prevents the door from being directly under an unbroken wall segment.
                for (int x = left; x <= right; x++) {
                    if (map[upperLayer][x] != SWITCH && map[upperLayer][x] != EXIT) map[upperLayer][x] = EMPTY;
                }
            }

            // The tile immediately above the door and its neighboring air space must be empty;
            // otherwise an opened door can still be visually present but practically unreachable.
            for (int y = doorY - 3; y < doorY; y++) {
                for (int x = doorX - 2; x <= doorX + 2; x++) {
                    if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= HEIGHT - 1) continue;
                    if (map[y][x] != SWITCH && map[y][x] != EXIT) map[y][x] = EMPTY;
                }
            }

            // Keep the actual door tile unchanged after clearing nearby tiles.
            map[doorY][doorX] = DOOR_CLOSED;
        }
    }

    void repairSwitchAccess() {
        // The switch still has a wall below it, but there must also be a little air space
        // around it so the character can land, stand on the switch, and leave again.
        for (auto& sw : switches) {
            int x = sw.x;
            int y = sw.y;

            clearSmallArea(x, y - 1, 1, 1);
            if (x - 1 > 0 && map[y][x - 1] != EXIT) map[y][x - 1] = EMPTY;
            if (x + 1 < WIDTH - 1 && map[y][x + 1] != EXIT) map[y][x + 1] = EMPTY;

            map[y][x] = SWITCH;
            if (y + 1 < HEIGHT - 1) map[y + 1][x] = WALL;
        }
    }

    void repairLocalSolvability(const std::vector<int>& layerRows) {
        // This repair step fixes local impossible structures without flattening the whole map.
        // It only opens narrow shafts around doors and small landing zones around switches.
        repairDoorAccess(layerRows);
        repairSwitchAccess();
    }

    void addLevelShapeVariation(std::mt19937& rng) {
        auto R2 = [&](int n) {
            std::uniform_int_distribution<int> dist(0, n - 1);
            return dist(rng);
        };

        int style = (level - 1) % 4;
        if (style == 1) {
            int islands = 3 + std::min(level, 6);
            for (int i = 0; i < islands; i++) {
                int y = 6 + R2(HEIGHT - 10);
                int x0 = 5 + R2(WIDTH - 16);
                int len = 4 + R2(7);
                for (int x = x0; x < x0 + len && x < WIDTH - 1; x++) {
                    if (map[y][x] == EMPTY) map[y][x] = WALL;
                }
            }
        } else if (style == 2) {
            int cols = 2 + std::min(level, 5);
            for (int i = 0; i < cols; i++) {
                int x = 5 + R2(WIDTH - 10);
                TileType t = (i % 2 == 0 ? WATER : LAVA);
                for (int y = 5; y < HEIGHT - 3; y++) {
                    if (R2(100) < 65 && map[y][x] != SWITCH && map[y][x] != EXIT) map[y][x] = t;
                }
            }
        } else if (style == 3) {
            int x = 6 + R2(8);
            for (int y = 5; y < HEIGHT - 3; y += 3) {
                for (int dx = 0; dx < 5; dx++) {
                    int cx = x + dx;
                    if (cx > 0 && cx < WIDTH - 1 && map[y][cx] == EMPTY) map[y][cx] = WALL;
                }
                x += 7;
                if (x > WIDTH - 12) x = 6 + R2(8);
            }
        }
    }

public:
    Game(int layers, int reward, int difficulty)
        : level(1), score(0), gameOver(false), levelComplete(false), gameCompleted(false),
          levelSeed(0), hasLevelSeed(false), terrainLayers(layers), rewardPoints(reward),
          difficultyId(difficulty), fixedLevelMode(false), fixedLevelId(0),
          recordedRandomMode(false), recordedRecordId(0), recordedRandomSeed(0),
          chooseOtherFixedLevel(false), chooseOtherRecordedLevel(false),
          levelStartScore(0) {
        term.enableRawMode();
        term.hideCursor();
        term.clearScreen();
        generateLevel();
    }

    Game(const FixedLevelConfig& fixedConfig)
        : level(fixedConfig.fixedLevelId), score(0), gameOver(false), levelComplete(false), gameCompleted(false),
          levelSeed(fixedConfig.seed), hasLevelSeed(true), terrainLayers(fixedConfig.terrainLayers),
          rewardPoints(fixedConfig.rewardPoints), difficultyId(fixedConfig.difficulty),
          fixedLevelMode(true), fixedLevelId(fixedConfig.fixedLevelId),
          recordedRandomMode(false), recordedRecordId(0), recordedRandomSeed(0),
          chooseOtherFixedLevel(false), chooseOtherRecordedLevel(false),
          levelStartScore(0) {
        term.enableRawMode();
        term.hideCursor();
        term.clearScreen();
        generateLevel(true);
    }

    Game(const RecordedRandomConfig& recordedConfig)
        : level(recordedConfig.levelNumber), score(0), gameOver(false), levelComplete(false), gameCompleted(false),
          levelSeed(recordedConfig.seed), hasLevelSeed(true), terrainLayers(recordedConfig.terrainLayers),
          rewardPoints(recordedConfig.rewardPoints), difficultyId(recordedConfig.difficulty),
          fixedLevelMode(false), fixedLevelId(0),
          recordedRandomMode(true), recordedRecordId(recordedConfig.recordId),
          recordedRandomSeed(recordedConfig.seed),
          chooseOtherFixedLevel(false), chooseOtherRecordedLevel(false),
          levelStartScore(0) {
        term.enableRawMode();
        term.hideCursor();
        term.clearScreen();
        generateLevel(true);
    }

    ~Game() {
        term.disableRawMode();
        term.showCursor();
        term.clearScreen();
    }

    bool isGameCompleted() const {
        return gameCompleted;
    }

    int getSessionScore() const {
        return score;
    }

    bool wantsOtherFixedLevel() const {
        return chooseOtherFixedLevel;
    }

    bool wantsOtherRecordedLevel() const {
        return chooseOtherRecordedLevel;
    }

    const std::vector<RandomLevelResult>& getRandomLevelResults() const {
        return randomLevelResults;
    }

    void restoreTerminalForMenu() {
        term.disableRawMode();
        term.showCursor();
        tcflush(STDIN_FILENO, TCIFLUSH);
    }

    void generateLevel(bool reuseSeed = false) {
        tcflush(STDIN_FILENO, TCIFLUSH);
        gameOver = false;
        levelComplete = false;
        switches.clear();
        map = std::vector<std::vector<TileType>>(HEIGHT, std::vector<TileType>(WIDTH, EMPTY));

        if (fixedLevelMode) {
            levelSeed = FIXED_LEVEL_SEEDS[fixedLevelId];
            hasLevelSeed = true;
        } else if (recordedRandomMode) {
            levelSeed = recordedRandomSeed;
            hasLevelSeed = true;
        } else if (!reuseSeed || !hasLevelSeed) {
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

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) map[y][x] = WALL;
            }
        }

        std::vector<int> layerRows;
        int pattern = (level - 1) % 4;
        if (terrainLayers == 3) {
            if (pattern == 0) layerRows = {5, 13, 21};
            else if (pattern == 1) layerRows = {6, 14, 21};
            else if (pattern == 2) layerRows = {4, 11, 19};
            else layerRows = {7, 15, 22};
        } else if (terrainLayers == 5) {
            if (pattern == 0) layerRows = {5, 9, 13, 17, 21};
            else if (pattern == 1) layerRows = {4, 8, 12, 16, 21};
            else if (pattern == 2) layerRows = {6, 10, 14, 18, 22};
            else layerRows = {3, 8, 13, 18, 22};
        } else {
            if (pattern == 0) layerRows = {3, 6, 9, 12, 15, 18, 21};
            else if (pattern == 1) layerRows = {4, 7, 10, 13, 16, 19, 22};
            else if (pattern == 2) layerRows = {3, 7, 11, 14, 17, 20, 22};
            else layerRows = {5, 8, 11, 14, 17, 20, 22};
        }

        int coopChance = std::min(75, 20 + level * 10);
        int gapCount = 2 + std::min(3, level / 2);
        int wallChance = std::max(25, 48 - level * 2);
        int waterChance = std::min(30, 14 + level);
        int lavaChance = std::min(30, 14 + level);

        for (int y : layerRows) {
            bool coopLayer = (level >= 2 && R(100) < coopChance);

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
                if (swY > 0 && swY < HEIGHT - 2) {
                    map[swY][swX] = SWITCH;
                    // Every switch gets a wall directly below it, so a character can stand on it.
                    map[swY + 1][swX] = WALL;
                    switches.push_back({swX, swY, doorX, y, OFF});
                }
            } else {
                std::vector<int> gaps;
                for (int g = 0; g < gapCount; g++) {
                    int candidate = 4 + R(WIDTH - 8);
                    bool farEnough = true;
                    for (int oldGap : gaps) {
                        if (std::abs(candidate - oldGap) < 5) farEnough = false;
                    }
                    if (farEnough) gaps.push_back(candidate);
                }

                for (int x = 1; x < WIDTH - 1; x++) {
                    bool isGap = false;
                    for (int gx : gaps) {
                        if (x == gx || (level >= 4 && std::abs(x - gx) == 1 && R(100) < 8)) {
                            isGap = true;
                            break;
                        }
                    }
                    if (isGap) continue;

                    int r = R(100);
                    if (r < wallChance) map[y][x] = WALL;
                    else if (r < wallChance + waterChance) map[y][x] = WATER;
                    else if (r < wallChance + waterChance + lavaChance) map[y][x] = LAVA;
                    else map[y][x] = SPIKE;
                }
            }
        }

        int numVerticalCols = 1 + level + terrainLayers / 3;
        for (int i = 0; i < numVerticalCols; i++) {
            int vx = R(WIDTH - 10) + 5;
            int startY = 2 + R(HEIGHT / 2);
            int len = 4 + R(6 + std::min(level, 8));
            int typeRoll = R(3);
            TileType vType = (typeRoll == 0 ? WALL : (typeRoll == 1 ? WATER : LAVA));

            for (int dy = 0; dy < len && (startY + dy) < HEIGHT - 1; dy++) {
                int cy = startY + dy;
                if (cy > 3 && cy < HEIGHT - 3 && map[cy][vx] != DOOR_CLOSED && map[cy][vx] != SWITCH) {
                    if (dy % 4 != 0) map[cy][vx] = vType;
                }
            }
        }

        // Add visible level-to-level variation before fairness cleanup.
        addLevelShapeVariation(rng);

        // After all random obstacles are placed, clean the areas directly below openings.
        // This keeps every fall route controllable instead of creating unavoidable instant deaths.
        makeFallRoutesFair(layerRows);

        // Fix local impossible map cases while preserving overall difficulty.
        repairLocalSolvability(layerRows);

        fireBoy = {2, 2, 2, 2, true, true, false};
        iceGirl = {WIDTH - 3, 2, WIDTH - 3, 2, false, false, false};

        for (int dy = 0; dy < 3; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                map[1 + dy][1 + dx] = EMPTY;
                map[1 + dy][WIDTH - 2 - dx] = EMPTY;
            }
        }

        // Safe starting platforms. With gravity enabled, both characters need
        // a small wall support under the cleared starting area.
        for (int dx = 0; dx < 3; dx++) {
            map[4][1 + dx] = WALL;
            map[4][WIDTH - 2 - dx] = WALL;
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

    bool processInput() {
        char key = term.readKey();
        if (key == 0) return false;
        if (key == 9) { toggleControl(); return true; }

        // During normal play, R now restarts the current level using the same seed,
        // so the map does not change.
        if (key == 'r' || key == 'R') {
            fireBoy.isDead = false;
            iceGirl.isDead = false;
            gameOver = false;
            generateLevel(true);
            return true;
        }

        if (key == 'q' || key == 'Q') { gameOver = true; return true; }

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

        bool moved = false;
        if (dx != 0 || dy != 0) {
            moved = tryMovePlayer(*p, dx, dy);
        }
        if (fireBoy.isDead || iceGirl.isDead) gameOver = true;
        return moved;
    }

    void draw() {
        std::stringstream ss;
        ss << "\033[H";
        ss << term.getColorStr(255, 255, 255);
        ss << "╔══════════════════════════════════════════════════════════════════╗\n";
        std::string levelText;
        if (fixedLevelMode) levelText = "Fixed " + std::to_string(fixedLevelId);
        else if (recordedRandomMode && recordedRecordId > 0) levelText = "Saved " + std::to_string(recordedRecordId);
        else if (recordedRandomMode) levelText = "Seed " + std::to_string(recordedRandomSeed);
        else levelText = std::to_string(level);
        if (levelText.length() > 12) levelText = levelText.substr(0, 12);
        if (levelText.length() < 12) levelText.append(12 - levelText.length(), ' ');
        std::string scoreText = std::to_string(score);
        if (scoreText.length() < 8) scoreText.append(8 - scoreText.length(), ' ');
        ss << "║  🎮 SSH Ice & Fire  │  Level: " << levelText
           << " │  Score: " << scoreText << "  ║\n";
        ss << "║  Tab: Switch │ WASD/Arrows: Move/Fall Control │ R: Retry │ Q: Quit ║\n";
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
        std::string completeText;
        if (fixedLevelMode) completeText = "FIXED LEVEL " + std::to_string(fixedLevelId);
        else if (recordedRandomMode && recordedRecordId > 0) completeText = "SAVED LEVEL " + std::to_string(recordedRecordId);
        else if (recordedRandomMode) completeText = "SEED LEVEL";
        else completeText = "LEVEL " + std::to_string(level);
        if (completeText.length() > 17) completeText = completeText.substr(0, 17);
        if (completeText.length() < 17) completeText.append(17 - completeText.length(), ' ');
        std::cout << "\n\n" << term.getColorStr(50, 255, 50)
                  << "    ╔═══════════════════════════════════════╗\n"
                  << "    ║                                       ║\n"
                  << "    ║     🎉 " << completeText << " COMPLETE! 🎉  ║\n"
                  << "    ║                                       ║\n"
                  << "    ║     Score Bonus: +" << (level * 100) << " points                ║\n"
                  << "    ║                                       ║\n"
                  << "    ╚═══════════════════════════════════════╝\n" << term.getResetStr() << "\n";
        usleep(1500000);
        score += level * 100;
        term.clearScreen();
    }

    void showGameOver() {
        while (true) {
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
                      << "    ╚═══════════════════════════════════════╝\n" << term.getResetStr() << "\n";

            if (fixedLevelMode) {
                std::cout << "    Press 'r' to replay same fixed level\n"
                          << "    Press 'o' to choose another fixed level\n"
                          << "    Press 'q' to quit\n\n";
            } else if (recordedRandomMode) {
                if (recordedRecordId > 0) {
                    std::cout << "    Press 'r' to replay this recorded random level\n";
                } else {
                    std::cout << "    Press 'r' to replay this seed-loaded random level\n";
                }
                std::cout << "    Press 'o' to choose another recorded level or seed\n"
                          << "    Press 'q' to quit\n\n";
            } else {
                std::cout << "    Press 'r' to replay same map\n"
                          << "    Press 'n' to restart from level 1 with a new random map\n"
                          << "    Press 'q' to quit\n\n";
            }

            term.showCursor();
            char c = 0;
            read(STDIN_FILENO, &c, 1);
            term.hideCursor();

            if (c == 'r' || c == 'R') {
                // Replay the exact same failed level map.
                fireBoy.isDead = false;
                iceGirl.isDead = false;
                gameOver = false;
                generateLevel(true);
                run();
                return;
            }

            if (!fixedLevelMode && !recordedRandomMode && (c == 'n' || c == 'N')) {
                // Start a completely new random game from level 1 with a new random map.
                score = 0;
                level = 1;
                hasLevelSeed = false;
                randomLevelResults.clear();
                levelStartScore = 0;
                fireBoy.isDead = false;
                iceGirl.isDead = false;
                gameOver = false;
                generateLevel(false);
                run();
                return;
            }

            if (fixedLevelMode && (c == 'o' || c == 'O')) {
                chooseOtherFixedLevel = true;
                return;
            }

            if (recordedRandomMode && (c == 'o' || c == 'O')) {
                chooseOtherRecordedLevel = true;
                return;
            }

            if (c == 'q' || c == 'Q') {
                return;
            }
        }
    }

    void run() {
        bool needRedraw = true;
        auto lastFallTime = std::chrono::steady_clock::now();

        while (!gameOver) {
            bool changed = processInput();

            auto now = std::chrono::steady_clock::now();
            long long elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFallTime).count();
            if (elapsedUs >= FALL_INTERVAL_US) {
                if (applyGravity()) changed = true;
                lastFallTime = now;
            }

            if (changed) updateMechanics();

            if (levelComplete) {
                int completedLevel = level;
                unsigned int completedSeed = levelSeed;
                int startScoreForThisLevel = levelStartScore;

                showLevelComplete();

                if (!fixedLevelMode && !recordedRandomMode) {
                    RandomLevelResult result;
                    result.difficulty = difficultyId;
                    result.terrainLayers = terrainLayers;
                    result.rewardPoints = rewardPoints;
                    result.levelNumber = completedLevel;
                    result.seed = completedSeed;
                    result.scoreEarned = score - startScoreForThisLevel;
                    result.completed = true;
                    randomLevelResults.push_back(result);
                    levelStartScore = score;
                }

                if (fixedLevelMode || recordedRandomMode) {
                    // Fixed and recorded-random modes contain one selected map.
                    // Completing it ends the current session instead of advancing randomly.
                    gameCompleted = true;
                    return;
                }

                level++;
                if (level > MAX_LEVELS) {
                    gameCompleted = true;
                    return;
                }
                generateLevel(false);
                lastFallTime = std::chrono::steady_clock::now();
                needRedraw = true;
            } else {
                if (changed || needRedraw) {
                    draw();
                    needRedraw = false;
                }

                usleep(15000);
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
        << "Select game mode after login:\n"
        << "  1) Random Challenge Mode - original 20-level procedural mode\n"
        << "  2) Fixed Level Mode - 10 prepared levels with saved progress\n"
        << "  3) Reload Random Level - replay by saved record ID or by seed\n\n"
        << "Fixed Level Distribution:\n"
        << "  Difficulty 1: Fixed Levels 1-3  (3 terrain layers)\n"
        << "  Difficulty 2: Fixed Levels 4-6  (5 terrain layers)\n"
        << "  Difficulty 3: Fixed Levels 7-10 (7 terrain layers)\n\n"
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

int inputGameMode() {
    while (true) {
        std::cout << "Choose game mode - 1 Random, 2 Fixed, 3 Reload Random ID/Seed: ";
        char c;
        std::cin >> c;
        if (c == '1') return 1;
        if (c == '2') return 2;
        if (c == '3') return 3;
        std::cout << "Invalid input. Please enter 1, 2, or 3.\n";
    }
}

int inputDifficulty() {
    while (true) {
        std::cout << "Enter difficulty 1, 2, or 3 to start random mode: ";
        char c;
        std::cin >> c;
        if (c == '1') return 1;
        if (c == '2') return 2;
        if (c == '3') return 3;
        std::cout << "Invalid input. Please enter 1, 2, or 3.\n";
    }
}

int inputFixedLevel(int savedLevel) {
    while (true) {
        std::cout << "\nFixed Level Mode\n";
        std::cout << "  Choose a fixed level from 1 to 10.\n";
        std::cout << "  Enter 0 to continue from your saved level: " << savedLevel << "\n";
        std::cout << "  New players start from fixed level 1 by default.\n";
        std::cout << "Your choice: ";

        int fixedLevelId = -1;
        if (std::cin >> fixedLevelId) {
            if (fixedLevelId == 0) return savedLevel;
            if (fixedLevelId >= 1 && fixedLevelId <= FIXED_LEVEL_COUNT) return fixedLevelId;
        }

        std::cout << "Invalid input. Please enter 0 or a number from 1 to 10.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void showRandomRecords(const std::vector<RandomLevelRecord>& records) {
    std::cout << "\nRecorded random levels for this player:\n";
    if (records.empty()) {
        std::cout << "  No recorded random levels yet.\n";
        return;
    }

    std::cout << "  ID | Diff | Game Level | Layers | Seed       | Score | Result\n";
    std::cout << "  ---------------------------------------------------------------\n";

    size_t limit = std::min<size_t>(records.size(), 20);
    for (size_t i = 0; i < limit; i++) {
        const auto& r = records[i];
        std::cout << "  " << r.recordId
                  << " | " << r.difficulty
                  << "    | " << r.levelNumber
                  << "          | " << r.terrainLayers
                  << "      | " << r.seed
                  << " | " << r.scoreEarned
                  << "   | " << (r.completed ? "Completed" : "Failed") << "\n";
    }

    if (records.size() > limit) {
        std::cout << "  ... showing newest 20 records only.\n";
    }
}

int inputRecordedRandomId(const std::vector<RandomLevelRecord>& records) {
    while (true) {
        showRandomRecords(records);
        std::cout << "\nEnter a record ID to reload, or 0 to cancel: ";

        int recordId = 0;
        if (std::cin >> recordId) {
            if (recordId == 0) return 0;
            for (const auto& record : records) {
                if (record.recordId == recordId) return recordId;
            }
        }

        std::cout << "Invalid record ID. Please choose an ID from the list.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int inputRandomReloadMethod(const std::vector<RandomLevelRecord>& records) {
    while (true) {
        showRandomRecords(records);
        std::cout << "\nReload Random Level Mode\n";
        std::cout << "  1) Reload by saved record ID\n";
        std::cout << "  2) Reload by seed\n";
        std::cout << "  0) Cancel\n";
        std::cout << "Your choice: ";

        char c;
        std::cin >> c;
        if (c == '0') return 0;
        if (c == '1') return 1;
        if (c == '2') return 2;

        std::cout << "Invalid input. Please enter 0, 1, or 2.\n";
    }
}

unsigned int inputRandomSeed() {
    while (true) {
        std::cout << "Enter the random level seed to reload: ";
        unsigned long long seedValue = 0;

        if (std::cin >> seedValue && seedValue <= std::numeric_limits<unsigned int>::max()) {
            return static_cast<unsigned int>(seedValue);
        }

        std::cout << "Invalid seed. Please enter a non-negative integer within unsigned int range.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int inputRandomGameLevelNumber() {
    while (true) {
        std::cout << "Enter the original game level number for this seed (1-" << MAX_LEVELS << "): ";
        int levelNumber = 1;

        if (std::cin >> levelNumber && levelNumber >= 1 && levelNumber <= MAX_LEVELS) {
            return levelNumber;
        }

        std::cout << "Invalid level number. Please enter a number from 1 to " << MAX_LEVELS << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void waitForEnter() {
    std::cout << "Press Enter to start..." << std::flush;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

int main() {
    RankingSystem ranking;
    ranking.loadFromFile(RANKING_FILE);

    FixedProgressSystem fixedProgress;
    fixedProgress.loadFromFile(FIXED_PROGRESS_FILE);

    RandomLevelArchive randomArchive;
    randomArchive.loadFromFile(RANDOM_LEVEL_RECORD_FILE);

    while (true) {
        showMainMenu(ranking);

        std::cout << "Please input your username.\n";
        std::cout << "Use this same username in future logins to keep your score record and fixed-level progress.\n\n";

        std::string username = inputUsername();
        int userIndex = ranking.loginOrRegister(username);
        ranking.saveToFile(RANKING_FILE);

        std::cout << "\nWelcome, " << ranking.getUsername(userIndex)
                  << ". Current total score: " << ranking.getScore(userIndex) << "\n";
        std::cout << "Saved fixed-level progress: Level "
                  << fixedProgress.getLastFixedLevel(username) << "\n\n";

        int gameMode = inputGameMode();

        bool completed = false;
        int earnedScore = 0;
        int savedRandomRecordCount = 0;

        if (gameMode == 1) {
            int difficulty = inputDifficulty();

            DifficultyConfig difficultyConfig = getDifficultyConfig(difficulty);

            {
                Game game(difficultyConfig.terrainLayers, difficultyConfig.rewardPoints, difficultyConfig.difficulty);
                game.run();
                completed = game.isGameCompleted();
                earnedScore = game.getSessionScore();

                const auto& results = game.getRandomLevelResults();
                for (const auto& result : results) {
                    randomArchive.addRecord(username, result);
                    savedRandomRecordCount++;
                }

                game.restoreTerminalForMenu();
            }

            if (savedRandomRecordCount > 0) {
                randomArchive.saveToFile(RANDOM_LEVEL_RECORD_FILE);
            }
        } else if (gameMode == 2) {
            bool chooseAnotherFixedLevel = true;

            while (chooseAnotherFixedLevel) {
                int savedLevel = fixedProgress.getLastFixedLevel(username);
                int fixedLevelId = inputFixedLevel(savedLevel);
                FixedLevelConfig fixedConfig = getFixedLevelConfig(fixedLevelId);

                fixedProgress.setLastFixedLevel(username, fixedLevelId);
                fixedProgress.saveToFile(FIXED_PROGRESS_FILE);

                std::cout << "\nSelected Fixed Level " << fixedConfig.fixedLevelId
                          << " | Difficulty " << fixedConfig.difficulty
                          << " | Terrain layers: " << fixedConfig.terrainLayers << "\n";
                std::cout << "If this level is completed, your saved progress will move to the next fixed level.\n";
                waitForEnter();

                bool currentCompleted = false;
                int currentEarnedScore = 0;
                bool requestedOtherLevel = false;

                {
                    Game game(fixedConfig);
                    game.run();
                    currentCompleted = game.isGameCompleted();
                    currentEarnedScore = game.getSessionScore();
                    requestedOtherLevel = game.wantsOtherFixedLevel();
                    game.restoreTerminalForMenu();
                }

                earnedScore += currentEarnedScore;
                completed = completed || currentCompleted;

                if (currentCompleted) {
                    int nextLevel = (fixedLevelId < FIXED_LEVEL_COUNT) ? fixedLevelId + 1 : FIXED_LEVEL_COUNT;
                    fixedProgress.setLastFixedLevel(username, nextLevel);
                    fixedProgress.saveToFile(FIXED_PROGRESS_FILE);
                }

                chooseAnotherFixedLevel = requestedOtherLevel;
            }
        } else {
            bool chooseAnotherRecord = true;

            while (chooseAnotherRecord) {
                std::vector<RandomLevelRecord> records = randomArchive.getUserRecords(username);
                int reloadMethod = inputRandomReloadMethod(records);
                if (reloadMethod == 0) break;

                RecordedRandomConfig config;
                bool configReady = false;

                if (reloadMethod == 1) {
                    if (records.empty()) {
                        std::cout << "\nNo recorded random level IDs found for this player. Use seed reload instead, or play random mode first.\n";
                        continue;
                    }

                    int recordId = inputRecordedRandomId(records);
                    if (recordId == 0) continue;

                    RandomLevelRecord selectedRecord;
                    if (!randomArchive.findRecordByIdForUser(username, recordId, selectedRecord)) {
                        std::cout << "Record not found. Returning to reload menu.\n";
                        continue;
                    }

                    config.recordId = selectedRecord.recordId;
                    config.difficulty = selectedRecord.difficulty;
                    config.terrainLayers = selectedRecord.terrainLayers;
                    config.rewardPoints = selectedRecord.rewardPoints;
                    config.levelNumber = selectedRecord.levelNumber;
                    config.seed = selectedRecord.seed;
                    configReady = true;

                    std::cout << "\nReloading Recorded Random Level ID " << config.recordId
                              << " | Original game level: " << config.levelNumber
                              << " | Difficulty " << config.difficulty
                              << " | Seed: " << config.seed << "\n";
                } else if (reloadMethod == 2) {
                    unsigned int seed = inputRandomSeed();

                    RandomLevelRecord matchedRecord;
                    if (randomArchive.findNewestRecordBySeedForUser(username, seed, matchedRecord)) {
                        config.recordId = matchedRecord.recordId;
                        config.difficulty = matchedRecord.difficulty;
                        config.terrainLayers = matchedRecord.terrainLayers;
                        config.rewardPoints = matchedRecord.rewardPoints;
                        config.levelNumber = matchedRecord.levelNumber;
                        config.seed = matchedRecord.seed;

                        std::cout << "\nSeed found in your archive. Using saved record ID " << config.recordId
                                  << " | Original game level: " << config.levelNumber
                                  << " | Difficulty " << config.difficulty
                                  << " | Seed: " << config.seed << "\n";
                    } else {
                        std::cout << "\nThis seed was not found in your saved records.\n";
                        std::cout << "To reproduce the map, please also provide the difficulty and original game level number.\n";
                        int difficulty = inputDifficulty();
                        int levelNumber = inputRandomGameLevelNumber();
                        DifficultyConfig difficultyConfig = getDifficultyConfig(difficulty);

                        config.recordId = 0;
                        config.difficulty = difficultyConfig.difficulty;
                        config.terrainLayers = difficultyConfig.terrainLayers;
                        config.rewardPoints = difficultyConfig.rewardPoints;
                        config.levelNumber = levelNumber;
                        config.seed = seed;

                        std::cout << "\nReloading by raw seed " << config.seed
                                  << " | Game level: " << config.levelNumber
                                  << " | Difficulty " << config.difficulty
                                  << " | Layers: " << config.terrainLayers << "\n";
                    }
                    configReady = true;
                }

                if (!configReady) continue;
                waitForEnter();

                bool currentCompleted = false;
                int currentEarnedScore = 0;
                bool requestedOtherRecord = false;

                {
                    Game game(config);
                    game.run();
                    currentCompleted = game.isGameCompleted();
                    currentEarnedScore = game.getSessionScore();
                    requestedOtherRecord = game.wantsOtherRecordedLevel();
                    game.restoreTerminalForMenu();
                }

                earnedScore += currentEarnedScore;
                completed = completed || currentCompleted;
                chooseAnotherRecord = requestedOtherRecord;
            }
        }

        if (earnedScore > 0) {
            ranking.addScore(userIndex, earnedScore);
            ranking.saveToFile(RANKING_FILE);
        }

        std::cin.clear();
        std::cout << "\033[2J\033[H" << std::flush;
        std::cout << "Game finished.\n";
        std::cout << "Player: " << ranking.getUsername(userIndex) << "\n";
        std::cout << "Added score: +" << earnedScore;
        if (completed) std::cout << " (at least one selected mode was completed)";
        std::cout << "\n";
        if (savedRandomRecordCount > 0) {
            std::cout << "Saved random level records: " << savedRandomRecordCount << "\n";
        }
        std::cout << "Current fixed-level progress: Level "
                  << fixedProgress.getLastFixedLevel(username) << "\n";
        std::cout << "New total score: " << ranking.getScore(userIndex) << "\n\n";
        std::cout << "Press y to return to main menu, or any other key to quit: " << std::flush;

        char again = 0;
        std::cin >> again;
        if (again != 'y' && again != 'Y') break;
    }

    ranking.saveToFile(RANKING_FILE);
    fixedProgress.saveToFile(FIXED_PROGRESS_FILE);
    randomArchive.saveToFile(RANDOM_LEVEL_RECORD_FILE);
    return 0;
}
