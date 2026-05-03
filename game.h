// game.h
// Game core: map generation, entity control, and frame loop
#ifndef GAME_H
#define GAME_H

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include "config.h"
#include "terminal.h"
#include "fixed_level.h"
#include "random_archive.h"

// ========== Game Tile Types ==========
enum TileType {
    EMPTY, WATER, LAVA, EXIT,
    SPIKE, DOOR_CLOSED, DOOR_OPEN,
    SWITCH,
    WALL
};

// ========== Switch State ==========
enum SwitchState { OFF, ON };

// ========== Switch Structure ==========
struct Switch {
    int x, y;
    int linkedDoorX, linkedDoorY;
    SwitchState state;
};

// ========== Player Character ==========
struct Player {
    int x, y;
    int prevX, prevY;
    bool isFire;    // true = Fire Boy, false = Ice Girl
    bool active;    // Currently controlled character
    bool isDead;
};

// ========== Main Game Class ==========
class Game {
public:
    /**
     * Constructor: Random challenge mode (based on difficulty config)
     * 
     * What it does: Initializes a new random level with specified parameters.
     * Inputs: layers     - Number of terrain layers
     *         reward     - Reward points for completing the level
     *         difficulty - Difficulty level (1-3)
     * Output: None
     */
    Game(int layers, int reward, int difficulty);

    /**
     * Constructor: Fixed level mode
     * 
     * What it does: Initializes a specific fixed level using pre-defined configuration.
     * Inputs: fixedConfig - Configuration for the fixed level
     * Output: None
     */
    Game(const FixedLevelConfig& fixedConfig);

    /**
     * Constructor: Replay a recorded random level
     * 
     * What it does: Replays a previously saved random level using recorded configuration.
     * Inputs: recordedConfig - Configuration from saved record
     * Output: None
     */
    Game(const RecordedRandomConfig& recordedConfig);

    ~Game();

    /**
     * Run the main game loop
     * 
     * What it does: Starts and runs the complete game session until completion or exit.
     * Inputs: None
     * Output: None
     */
    void run();

    /**
     * Check if the entire level chain is completed
     * 
     * What it does: Returns whether the player has finished all levels (e.g. all 20 random levels).
     * Inputs: None
     * Output: true if game is fully completed
     */
    bool isGameCompleted() const;

    /**
     * Get score earned in current session
     * 
     * What it does: Returns the total score accumulated in this game session.
     * Inputs: None
     * Output: Current session score
     */
    int getSessionScore() const;

    /**
     * Get current fixed level ID (only meaningful in fixed mode)
     * 
     * What it does: Returns the fixed level number if in fixed mode, otherwise 0.
     * Inputs: None
     * Output: Fixed level ID or 0
     */
    int getCurrentFixedLevelId() const;

    /**
     * Check if player wants to choose another fixed level
     * 
     * What it does: Indicates if the player requested to switch to a different fixed level after completion.
     * Inputs: None
     * Output: true if player wants another fixed level
     */
    bool wantsOtherFixedLevel() const;

    /**
     * Check if player wants to choose another recorded level
     * 
     * What it does: Indicates if the player requested to switch to a different recorded level.
     * Inputs: None
     * Output: true if player wants another recorded level
     */
    bool wantsOtherRecordedLevel() const;

    /**
     * Get all random level results from this session
     * 
     * What it does: Returns records of all random levels played in this session for persistence.
     * Inputs: None
     * Output: Vector of RandomLevelResult
     */
    const std::vector<RandomLevelResult>& getRandomLevelResults() const;

    /**
     * Restore terminal settings for menu display
     * 
     * What it does: Temporarily disables raw mode and shows cursor for menu interaction.
     * Inputs: None
     * Output: None
     */
    void restoreTerminalForMenu();

private:
    Terminal term;

    int level;               // Current level number
    int score;               // Current score
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

    // Helper functions
    bool isBeginnerDifficulty() const;
    int getLayerGapHalfWidth() const;
    int getMinimumGapDistance() const;
    bool isBlockingTile(TileType tile) const;
    bool isHazardForPlayer(const Player& p, TileType tile) const;
    void checkCurrentTile(Player& p);
    bool applyGravityToPlayer(Player& p);
    bool applyGravity();
    bool tryMovePlayer(Player& p, int dx, int dy);

    bool canOverwriteForFallSafety(TileType tile) const;
    void clearFairFallSpaceAroundOpening(int openingX, int layerY);
    void makeFallRoutesFair(const std::vector<int>& layerRows);
    void clearSmallArea(int cx, int cy, int radiusX, int radiusY);
    void repairDoorAccess(const std::vector<int>& layerRows);
    void repairSwitchAccess();
    void repairLocalSolvability(const std::vector<int>& layerRows);
    void addLevelShapeVariation(std::mt19937& rng);
    void loadFixedLevelState(int newFixedLevelId);

    void generateLevel(bool reuseSeed = false);
    void toggleControl();
    void updateMechanics();
    bool processInput();
    void draw();
    void showLevelComplete();
    void showGameOver();
};

#endif // GAME_H
