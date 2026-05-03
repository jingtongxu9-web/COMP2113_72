// utils.h
// UI utilities: text formatting, menu display, user input handling
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include "ranking.h"
#include "random_archive.h"
#include "fixed_level.h"

// Text formatting utilities
std::string fitText(const std::string& text, size_t width);
std::string fitNumber(int value, size_t width);

/**
 * Display the main menu
 * 
 * What it does: Clears screen and shows the main game menu with top players.
 * Inputs: ranking - Reference to the ranking system
 * Output: None
 */
void showMainMenu(const RankingSystem& ranking);

/**
 * Get username from user input
 * 
 * What it does: Prompts user to enter their username.
 * Inputs: None
 * Output: Username string entered by user
 */
std::string inputUsername();

/**
 * Get game mode selection from user
 * 
 * What it does: Prompts user to choose between Random, Fixed, or Reload modes.
 * Inputs: None
 * Output: Selected mode (1, 2, or 3)
 */
int inputGameMode();

/**
 * Get difficulty level from user
 * 
 * What it does: Prompts user to select difficulty for random mode.
 * Inputs: None
 * Output: Difficulty level (1, 2, or 3)
 */
int inputDifficulty();

/**
 * Get fixed level selection from user
 * 
 * What it does: Prompts user to choose a fixed level or continue from saved progress.
 * Inputs: savedLevel - User's last completed fixed level
 * Output: Chosen fixed level number
 */
int inputFixedLevel(int savedLevel);

// Random level record display and selection
void showRandomRecords(const std::vector<RandomLevelRecord>& records);

/**
 * Get recorded random level ID from user
 * 
 * What it does: Shows records and lets user select one by ID.
 * Inputs: records - List of user's recorded random levels
 * Output: Selected record ID, or 0 to cancel
 */
int inputRecordedRandomId(const std::vector<RandomLevelRecord>& records);

/**
 * Choose method to reload random level
 * 
 * What it does: Lets user choose between reloading by record ID or by seed.
 * Inputs: records - List of user's recorded random levels
 * Output: User choice (0=cancel, 1=by ID, 2=by seed)
 */
int inputRandomReloadMethod(const std::vector<RandomLevelRecord>& records);

/**
 * Get random seed from user input
 * 
 * What it does: Prompts user to enter a seed value to reload a level.
 * Inputs: None
 * Output: Unsigned integer seed value
 */
unsigned int inputRandomSeed();

/**
 * Get original game level number for seed reload
 * 
 * What it does: Prompts user for the original level number when reloading by seed.
 * Inputs: None
 * Output: Level number (1 to MAX_LEVELS)
 */
int inputRandomGameLevelNumber();

/**
 * Wait for user to press Enter
 * 
 * What it does: Pauses execution until user presses Enter.
 * Inputs: None
 * Output: None
 */
void waitForEnter();

#endif
