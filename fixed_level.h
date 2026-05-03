// fixed_level.h
// Fixed level mode configuration, difficulty settings and progress tracking
#ifndef FIXED_LEVEL_H
#define FIXED_LEVEL_H

#include <string>
#include <vector>
#include <fstream>

// Total number of fixed levels
constexpr int FIXED_LEVEL_COUNT = 10;

// Fixed level seed table (index 0 is placeholder, 1~10 correspond to the ten fixed levels)
constexpr unsigned int FIXED_LEVEL_SEEDS[FIXED_LEVEL_COUNT + 1] = {
    0,
    12031, 24067, 36109,
    48221, 59333, 71441,
    83563, 95617, 107741, 118873
};

// Configuration for a single fixed level
struct FixedLevelConfig {
    int fixedLevelId;      // Level number 1~10
    int difficulty;        // Difficulty level 1~3
    int terrainLayers;     // Number of platform/terrain layers
    int rewardPoints;      // Reward points
    unsigned int seed;     // Random seed
};

// Difficulty configuration (used in random challenge mode)
struct DifficultyConfig {
    int difficulty;
    int terrainLayers;
    int rewardPoints;
};

/**
 * Get configuration for a specific fixed level
 * 
 * What it does: Returns the complete configuration (difficulty, terrain, reward, seed)
 *               for the given fixed level.
 * Inputs: fixedLevelId - Level number (1 to 10)
 * Output: FixedLevelConfig struct with all level parameters
 */
FixedLevelConfig getFixedLevelConfig(int fixedLevelId);

/**
 * Get configuration for a specific difficulty level
 * 
 * What it does: Returns terrain layers and reward points corresponding to a difficulty.
 * Inputs: difficulty - Difficulty level (1 to 3)
 * Output: DifficultyConfig struct
 */
DifficultyConfig getDifficultyConfig(int difficulty);

// Fixed level progress record (one line per player)
struct FixedProgressRecord {
    std::string username;
    int lastFixedLevel;    // Most recently completed fixed level number
};

// Fixed level progress management system
class FixedProgressSystem {
private:
    std::vector<FixedProgressRecord> records;

    /**
     * Find record index by username
     * 
     * What it does: Searches for a user in the records vector.
     * Inputs: username - Player's username
     * Output: Index of the record, or -1 if not found
     */
    int findUser(const std::string& username) const;

public:
    /**
     * Get the last completed fixed level for a user
     * 
     * What it does: Returns the highest fixed level the user has completed.
     * Inputs: username - Player's username
     * Output: Last completed level number (default: 1)
     */
    int getLastFixedLevel(const std::string& username) const;

    /**
     * Update the last completed fixed level for a user
     * 
     * What it does: Saves or updates the user's progress in fixed levels.
     * Inputs: username - Player's username
     *         levelId   - The level number to save
     * Output: None
     */
    void setLastFixedLevel(const std::string& username, int levelId);

    /**
     * Load progress data from file
     * 
     * What it does: Loads all players' fixed level progress from a text file.
     * Inputs: filename - Path to the progress file
     * Output: true if successful, false otherwise
     */
    bool loadFromFile(const std::string& filename);

    /**
     * Save progress data to file
     * 
     * What it does: Writes all current progress records to a text file.
     * Inputs: filename - Path to the progress file
     * Output: true if successful, false otherwise
     */
    bool saveToFile(const std::string& filename) const;
};

#endif // FIXED_LEVEL_H
