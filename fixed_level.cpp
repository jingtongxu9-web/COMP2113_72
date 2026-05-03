// fixed_level.cpp
// Implementation of fixed level configuration and progress system
#include "fixed_level.h"

// ========== Fixed Level Configuration ==========

/**
 * Get configuration for a specific fixed level
 * 
 * What it does: Returns the complete configuration for the requested fixed level.
 * Inputs: fixedLevelId - Level number (1 to 10)
 * Output: FixedLevelConfig struct
 */
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

// ========== Difficulty Configuration ==========

/**
 * Get configuration for a specific difficulty level
 * 
 * What it does: Returns terrain and reward settings based on difficulty.
 * Inputs: difficulty - Difficulty level (1 to 3)
 * Output: DifficultyConfig struct
 */
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

// ========== FixedProgressSystem Implementation ==========

/**
 * Find record index by username
 * 
 * What it does: Searches for a user in the internal records.
 * Inputs: username - Player's username
 * Output: Index of the record, or -1 if not found
 */
int FixedProgressSystem::findUser(const std::string& username) const {
    for (size_t i = 0; i < records.size(); i++) {
        if (records[i].username == username) return static_cast<int>(i);
    }
    return -1;
}

/**
 * Get the last completed fixed level for a user
 * 
 * What it does: Returns the user's progress in fixed levels.
 * Inputs: username - Player's username
 * Output: Last completed level number (default: 1)
 */
int FixedProgressSystem::getLastFixedLevel(const std::string& username) const {
    int idx = findUser(username);
    if (idx == -1) return 1;
    if (records[idx].lastFixedLevel < 1) return 1;
    if (records[idx].lastFixedLevel > FIXED_LEVEL_COUNT) return FIXED_LEVEL_COUNT;
    return records[idx].lastFixedLevel;
}

/**
 * Update the last completed fixed level for a user
 * 
 * What it does: Saves or updates the user's highest completed fixed level.
 * Inputs: username - Player's username
 *         levelId   - Level number to save
 * Output: None
 */
void FixedProgressSystem::setLastFixedLevel(const std::string& username, int levelId) {
    if (levelId < 1) levelId = 1;
    if (levelId > FIXED_LEVEL_COUNT) levelId = FIXED_LEVEL_COUNT;

    int idx = findUser(username);
    if (idx == -1) {
        records.push_back({username, levelId});
    } else {
        records[idx].lastFixedLevel = levelId;
    }
}

/**
 * Load progress data from file
 * 
 * What it does: Loads player progress from a text file.
 * Inputs: filename - Path to the progress file
 * Output: true if successful, false otherwise
 */
bool FixedProgressSystem::loadFromFile(const std::string& filename) {
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

/**
 * Save progress data to file
 * 
 * What it does: Writes all progress records to a text file.
 * Inputs: filename - Path to the progress file
 * Output: true if successful, false otherwise
 */
bool FixedProgressSystem::saveToFile(const std::string& filename) const {
    std::ofstream out(filename, std::ios::trunc);
    if (!out.is_open()) return false;

    out << records.size() << "\n";
    for (const auto& record : records) {
        out << record.username << " " << record.lastFixedLevel << "\n";
    }
    return true;
}
