// utils.cpp
// UI utilities implementation
#include "utils.h"
#include "config.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <cctype>

// ========== Text Formatting ==========

/**
 * Fit text to specified width
 * 
 * What it does: Truncates or pads string to exact width with ellipsis if needed.
 * Inputs: text  - Input string
 *         width - Target width
 * Output: Formatted string of exact width
 */
std::string fitText(const std::string& text, size_t width) {
    if (text.length() == width) return text;
    if (text.length() < width) return text + std::string(width - text.length(), ' ');
    if (width <= 3) return text.substr(0, width);
    return text.substr(0, width - 3) + "...";
}

/**
 * Format number right-aligned to specified width
 * 
 * What it does: Pads a number with spaces on the left to reach target width.
 * Inputs: value - Integer value
 *         width - Target width
 * Output: Right-aligned formatted string
 */
std::string fitNumber(int value, size_t width) {
    std::string text = std::to_string(value);
    if (text.length() >= width) return text;
    return std::string(width - text.length(), ' ') + text;
}

// ========== Main Menu ==========

/**
 * Display the main menu
 * 
 * What it does: Clears screen and shows the main game menu with top players.
 * Inputs: ranking - Reference to the ranking system
 * Output: None
 */
void showMainMenu(const RankingSystem& ranking) {
    std::cout << "\033[2J\033[H" << std::flush;
    std::cout
        << "============================================================\n"
        << "                       Frozen Spark                         \n"
        << "============================================================\n\n"
        << "Select game mode after login:\n"
        << "  1) Random Challenge Mode - original 20-level procedural mode\n"
        << "  2) Fixed Level Mode - 10 prepared levels with saved progress\n"
        << "  3) Reload Random Level - replay by saved record ID or by seed\n\n"
        << "Fixed Level Distribution:\n"
        << "  Difficulty 1: Fixed Levels 1-3  (3 terrain layers, wider openings)\n"
        << "  Difficulty 2: Fixed Levels 4-6  (5 terrain layers)\n"
        << "  Difficulty 3: Fixed Levels 7-10 (7 terrain layers)\n\n"
        << "Top 3 Players:\n";

    std::vector<PlayerRecord> top = ranking.getTopThree();
    if (top.empty()) {
        std::cout << "  No scored players yet.\n";
    } else {
        std::cout << "  Rank | Player               | Score\n";
        std::cout << "  --------------------------------------\n";
        for (size_t i = 0; i < top.size(); i++) {
            std::cout << "  " << fitNumber(static_cast<int>(i + 1), 4)
                      << " | " << fitText(top[i].username, 20)
                      << " | " << top[i].score << "\n";
        }
    }

    std::cout << "\n";
}

// ========== User Input Functions ==========

/**
 * Get username from user input
 * 
 * What it does: Prompts user to enter their username.
 * Inputs: None
 * Output: Username string entered by user
 */
std::string inputUsername() {
    std::string username;
    std::cout << "Enter your username: ";
    std::cin >> username;
    return username;
}

/**
 * Get game mode selection from user
 * 
 * What it does: Prompts user to choose between Random, Fixed, or Reload modes.
 * Inputs: None
 * Output: Selected mode (1, 2, or 3)
 */
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

/**
 * Get difficulty level from user
 * 
 * What it does: Prompts user to select difficulty for random mode.
 * Inputs: None
 * Output: Difficulty level (1, 2, or 3)
 */
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

/**
 * Get fixed level selection from user
 * 
 * What it does: Prompts user to choose a fixed level or continue from saved progress.
 * Inputs: savedLevel - User's last completed fixed level
 * Output: Chosen fixed level number
 */
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

// ========== Random Level Records ==========

/**
 * Display list of recorded random levels
 * 
 * What it does: Shows a formatted table of the player's saved random level records.
 * Inputs: records - Vector of random level records
 * Output: None
 */
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

/**
 * Get recorded random level ID from user
 * 
 * What it does: Shows records and lets user select one by ID.
 * Inputs: records - List of user's recorded random levels
 * Output: Selected record ID, or 0 to cancel
 */
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

/**
 * Choose method to reload random level
 * 
 * What it does: Lets user choose between reloading by record ID or by seed.
 * Inputs: records - List of user's recorded random levels
 * Output: User choice (0=cancel, 1=by ID, 2=by seed)
 */
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

/**
 * Get random seed from user input
 * 
 * What it does: Prompts user to enter a seed value to reload a level.
 * Inputs: None
 * Output: Unsigned integer seed value
 */
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

/**
 * Get original game level number for seed reload
 * 
 * What it does: Prompts user for the original level number when reloading by seed.
 * Inputs: None
 * Output: Level number (1 to MAX_LEVELS)
 */
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

// ========== Helper Functions ==========

/**
 * Wait for user to press Enter
 * 
 * What it does: Pauses execution until user presses Enter.
 * Inputs: None
 * Output: None
 */
void waitForEnter() {
    std::cout << "Press Enter to start..." << std::flush;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}
