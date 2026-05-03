// game.cpp
// Game core implementation: map generation, logic updates, rendering and main loop
#include "game.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <limits>

// ========== Constructor / Destructor ==========

/**
 * Constructor: Random challenge mode
 * 
 * What it does: Initializes game with random level parameters.
 * Inputs: layers, reward, difficulty - Level configuration
 * Output: None
 */
Game::Game(int layers, int reward, int difficulty)
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

/**
 * Constructor: Fixed level mode
 * 
 * What it does: Initializes a specific fixed level.
 * Inputs: fixedConfig - Fixed level configuration
 * Output: None
 */
Game::Game(const FixedLevelConfig& fixedConfig)
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

/**
 * Constructor: Replay recorded random level
 * 
 * What it does: Replays a saved random level from archive.
 * Inputs: recordedConfig - Recorded level configuration
 * Output: None
 */
Game::Game(const RecordedRandomConfig& recordedConfig)
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

Game::~Game() {
    term.disableRawMode();
    term.showCursor();
    term.clearScreen();
}

// ========== Public Property Accessors ==========

bool Game::isGameCompleted() const {
    return gameCompleted;
}

int Game::getSessionScore() const {
    return score;
}

int Game::getCurrentFixedLevelId() const {
    if (!fixedLevelMode) return 0;
    return fixedLevelId;
}

bool Game::wantsOtherFixedLevel() const {
    return chooseOtherFixedLevel;
}

bool Game::wantsOtherRecordedLevel() const {
    return chooseOtherRecordedLevel;
}

const std::vector<RandomLevelResult>& Game::getRandomLevelResults() const {
    return randomLevelResults;
}

void Game::restoreTerminalForMenu() {
    term.disableRawMode();
    term.showCursor();
    tcflush(STDIN_FILENO, TCIFLUSH);
}

// ========== Difficulty Helper Functions ==========

/**
 * Check if current difficulty is beginner
 * 
 * What it does: Determines if the game is in easiest difficulty mode.
 * Inputs: None
 * Output: true if beginner difficulty
 */
bool Game::isBeginnerDifficulty() const {
    return difficultyId == 1;
}

/**
 * Get half-width of gaps in layers
 * 
 * What it does: Returns gap size adjustment based on difficulty.
 * Inputs: None
 * Output: Gap half width
 */
int Game::getLayerGapHalfWidth() const {
    return isBeginnerDifficulty() ? 2 : 0;
}

/**
 * Get minimum distance between gaps
 * 
 * What it does: Returns minimum spacing requirement between gaps.
 * Inputs: None
 * Output: Minimum gap distance
 */
int Game::getMinimumGapDistance() const {
    return isBeginnerDifficulty() ? 9 : 5;
}

/**
 * Check if tile blocks movement
 * 
 * What it does: Determines if a tile is solid and cannot be passed through.
 * Inputs: tile - Tile type to check
 * Output: true if blocking
 */
bool Game::isBlockingTile(TileType tile) const {
    return tile == WALL || tile == DOOR_CLOSED;
}

/**
 * Check if tile is hazardous to player
 * 
 * What it does: Determines if the tile is dangerous for the current player type.
 * Inputs: p    - Player reference
 *         tile - Tile type
 * Output: true if hazardous
 */
bool Game::isHazardForPlayer(const Player& p, TileType tile) const {
    if (tile == SPIKE) return true;
    if (p.isFire && tile == WATER) return true;
    if (!p.isFire && tile == LAVA) return true;
    return false;
}

/**
 * Check effects of current tile on player
 * 
 * What it does: Applies hazards or exit conditions when player stands on a tile.
 * Inputs: p - Player reference
 * Output: None
 */
void Game::checkCurrentTile(Player& p) {
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

// ========== Gravity & Movement ==========

/**
 * Apply gravity to a single player
 * 
 * What it does: Makes a player fall if possible and checks for hazards.
 * Inputs: p - Player reference
 * Output: true if player moved
 */
bool Game::applyGravityToPlayer(Player& p) {
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

/**
 * Apply gravity to both players
 * 
 * What it does: Updates falling for both Fire Boy and Ice Girl.
 * Inputs: None
 * Output: true if any player moved
 */
bool Game::applyGravity() {
    bool changed = false;
    changed = applyGravityToPlayer(fireBoy) || changed;
    changed = applyGravityToPlayer(iceGirl) || changed;
    return changed;
}

/**
 * Attempt to move a player
 * 
 * What it does: Tries to move player in specified direction if valid.
 * Inputs: p  - Player reference
 *         dx - Delta X
 *         dy - Delta Y
 * Output: true if movement succeeded
 */
bool Game::tryMovePlayer(Player& p, int dx, int dy) {
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

// ========== Map Repair (Playability) ==========

/**
 * Check if tile can be overwritten for fall safety
 * 
 * What it does: Determines if a tile can be safely replaced with empty space.
 * Inputs: tile - Tile type
 * Output: true if can be overwritten
 */
bool Game::canOverwriteForFallSafety(TileType tile) const {
    return tile == EMPTY || tile == WATER || tile == LAVA || tile == SPIKE || tile == WALL;
}

/**
 * Clear safe fall space around openings
 * 
 * What it does: Ensures fair falling paths by clearing obstacles below gaps.
 * Inputs: openingX - X position of gap
 *         layerY   - Y position of layer
 * Output: None
 */
void Game::clearFairFallSpaceAroundOpening(int openingX, int layerY) {
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

/**
 * Make fall routes fair
 * 
 * What it does: Clears safe falling paths under gaps in terrain layers.
 * Inputs: layerRows - List of terrain layer Y positions
 * Output: None
 */
void Game::makeFallRoutesFair(const std::vector<int>& layerRows) {
    for (int layerY : layerRows) {
        for (int x = 1; x < WIDTH - 1; x++) {
            if (map[layerY][x] == EMPTY) {
                clearFairFallSpaceAroundOpening(x, layerY);
            }
        }
    }

    for (int x = 1; x <= 3; x++) clearFairFallSpaceAroundOpening(x, 4);
    for (int x = WIDTH - 4; x <= WIDTH - 2; x++) clearFairFallSpaceAroundOpening(x, 4);
}

/**
 * Clear a small rectangular area
 * 
 * What it does: Clears obstacles in a small area around a point.
 * Inputs: cx, cy     - Center coordinates
 *         radiusX, radiusY - Clearance radii
 * Output: None
 */
void Game::clearSmallArea(int cx, int cy, int radiusX, int radiusY) {
    for (int y = cy - radiusY; y <= cy + radiusY; y++) {
        for (int x = cx - radiusX; x <= cx + radiusX; x++) {
            if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= HEIGHT - 1) continue;
            if (map[y][x] == EXIT) continue;
            map[y][x] = EMPTY;
        }
    }
}

/**
 * Repair access to doors
 * 
 * What it does: Ensures paths to doors are clear for playability.
 * Inputs: layerRows - List of terrain layer positions
 * Output: None
 */
void Game::repairDoorAccess(const std::vector<int>& layerRows) {
    for (auto& sw : switches) {
        int doorX = sw.linkedDoorX;
        int doorY = sw.linkedDoorY;

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

            for (int x = left; x <= right; x++) {
                if (map[upperLayer][x] != SWITCH && map[upperLayer][x] != EXIT) map[upperLayer][x] = EMPTY;
            }
        }

        for (int y = doorY - 3; y < doorY; y++) {
            for (int x = doorX - 2; x <= doorX + 2; x++) {
                if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= HEIGHT - 1) continue;
                if (map[y][x] != SWITCH && map[y][x] != EXIT) map[y][x] = EMPTY;
            }
        }

        map[doorY][doorX] = DOOR_CLOSED;
    }
}

/**
 * Repair access to switches
 * 
 * What it does: Ensures switches are reachable by clearing surrounding area.
 * Inputs: None
 * Output: None
 */
void Game::repairSwitchAccess() {
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

/**
 * Repair local solvability issues
 * 
 * What it does: Fixes access to doors and switches for fair gameplay.
 * Inputs: layerRows - Terrain layer positions
 * Output: None
 */
void Game::repairLocalSolvability(const std::vector<int>& layerRows) {
    repairDoorAccess(layerRows);
    repairSwitchAccess();
}

// ========== Map Decoration ==========

/**
 * Add visual shape variation to level
 * 
 * What it does: Adds decorative elements and variety to the level layout.
 * Inputs: rng - Random number generator
 * Output: None
 */
void Game::addLevelShapeVariation(std::mt19937& rng) {
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

// ========== Fixed Level State Loading ==========

/**
 * Load configuration for a fixed level
 * 
 * What it does: Updates game state with configuration for a new fixed level.
 * Inputs: newFixedLevelId - Target fixed level number
 * Output: None
 */
void Game::loadFixedLevelState(int newFixedLevelId) {
    FixedLevelConfig cfg = getFixedLevelConfig(newFixedLevelId);
    fixedLevelId = cfg.fixedLevelId;
    level = cfg.fixedLevelId;
    terrainLayers = cfg.terrainLayers;
    rewardPoints = cfg.rewardPoints;
    difficultyId = cfg.difficulty;
    levelSeed = cfg.seed;
    hasLevelSeed = true;
}

// ========== Level Generation ==========

/**
 * Generate a new level
 * 
 * What it does: Creates the complete map layout, places entities and obstacles.
 * Inputs: reuseSeed - Whether to reuse the current level seed
 * Output: None
 */
void Game::generateLevel(bool reuseSeed) {
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

    int layerGapHalfWidth = getLayerGapHalfWidth();
    int minimumGapDistance = getMinimumGapDistance();
    if (isBeginnerDifficulty()) {
        gapCount = std::max(gapCount, 3);
    }

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
                map[swY + 1][swX] = WALL;
                switches.push_back({swX, swY, doorX, y, OFF});
            }
        } else {
            std::vector<int> gaps;
            for (int g = 0; g < gapCount; g++) {
                int candidate = 4 + R(WIDTH - 8);
                bool farEnough = true;
                for (int oldGap : gaps) {
                    if (std::abs(candidate - oldGap) < minimumGapDistance) farEnough = false;
                }
                if (farEnough) gaps.push_back(candidate);
            }

            for (int x = 1; x < WIDTH - 1; x++) {
                bool isGap = false;
                for (int gx : gaps) {
                    int distanceFromGapCenter = std::abs(x - gx);

                    if (distanceFromGapCenter <= layerGapHalfWidth) {
                        isGap = true;
                        break;
                    }

                    if (!isBeginnerDifficulty() && level >= 4 && distanceFromGapCenter == 1 && R(100) < 8) {
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

    addLevelShapeVariation(rng);
    makeFallRoutesFair(layerRows);
    repairLocalSolvability(layerRows);

    fireBoy = {2, 2, 2, 2, true, true, false};
    iceGirl = {WIDTH - 3, 2, WIDTH - 3, 2, false, false, false};

    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            map[1 + dy][1 + dx] = EMPTY;
            map[1 + dy][WIDTH - 2 - dx] = EMPTY;
        }
    }

    for (int dx = 0; dx < 3; dx++) {
        map[4][1 + dx] = WALL;
        map[4][WIDTH - 2 - dx] = WALL;
    }

    map[HEIGHT - 2][WIDTH / 2] = EXIT;
    map[HEIGHT - 3][WIDTH / 2] = EMPTY;
}

// ========== Controls & Mechanics ==========

/**
 * Toggle active player
 * 
 * What it does: Switches control between Fire Boy and Ice Girl.
 * Inputs: None
 * Output: None
 */
void Game::toggleControl() {
    fireBoy.active = !fireBoy.active;
    iceGirl.active = !fireBoy.active;
}

/**
 * Update game mechanics (mainly switches and doors)
 * 
 * What it does: Updates switch states and door openness based on player positions.
 * Inputs: None
 * Output: None
 */
void Game::updateMechanics() {
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

/**
 * Process keyboard input
 * 
 * What it does: Reads and handles player input for movement and actions.
 * Inputs: None
 * Output: true if game state changed
 */
bool Game::processInput() {
    char key = term.readKey();
    if (key == 0) return false;
    if (key == 9) { toggleControl(); return true; }

    if (key == 'r' || key == 'R') {
        fireBoy.isDead = false;
        iceGirl.isDead = false;
        gameOver = false;
        generateLevel(true);
        return true;
    }

    if ((key == 'f' || key == 'F') && !fixedLevelMode && !recordedRandomMode) {
        score = levelStartScore;
        fireBoy.isDead = false;
        iceGirl.isDead = false;
        gameOver = false;
        generateLevel(false);
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

// ========== Rendering ==========

/**
 * Draw the current game frame
 * 
 * What it does: Renders the full game screen including map, players, UI and legend.
 * Inputs: None
 * Output: None
 */
void Game::draw() {
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
    ss << "║  🎮 Frozen Spark    │  Level: " << levelText
       << " │  Score: " << scoreText << "  ║\n";
    if (!fixedLevelMode && !recordedRandomMode) {
        ss << "║  Tab: Switch │ Move/Fall: WASD/Arrows │ R: Retry │ F: New │ Q: Quit ║\n";
    } else {
        ss << "║  Tab: Switch │ WASD/Arrows: Move/Fall Control │ R: Retry │ Q: Quit ║\n";
    }
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

// ========== Level Complete / Game Over ==========

/**
 * Show level completion screen
 * 
 * What it does: Displays success message and awards bonus points.
 * Inputs: None
 * Output: None
 */
void Game::showLevelComplete() {
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

/**
 * Show game over screen and handle restart options
 * 
 * What it does: Displays game over message and provides restart/quit options.
 * Inputs: None
 * Output: None
 */
void Game::showGameOver() {
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
	ssize_t bytesRead=read(STDIN_FILENO, &c, 1);
	if (bytesRead!=1){
		c='\0';
	}
        term.hideCursor();

        if (c == 'r' || c == 'R') {
            fireBoy.isDead = false;
            iceGirl.isDead = false;
            gameOver = false;
            generateLevel(true);
            run();
            return;
        }

        if (!fixedLevelMode && !recordedRandomMode && (c == 'n' || c == 'N')) {
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

// ========== Main Loop ==========

/**
 * Main game loop
 * 
 * What it does: Handles input, gravity, mechanics, rendering, and level transitions.
 * Inputs: None
 * Output: None
 */
void Game::run() {
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

            if (fixedLevelMode) {
                if (fixedLevelId >= FIXED_LEVEL_COUNT) {
                    gameCompleted = true;
                    return;
                }

                loadFixedLevelState(fixedLevelId + 1);
                levelStartScore = score;
                generateLevel(true);
                lastFallTime = std::chrono::steady_clock::now();
                needRedraw = true;
                continue;
            }

            if (recordedRandomMode) {
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
