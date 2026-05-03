// main.cpp
// Entry point: game startup, mode selection, and session management
#include <iostream>
#include <limits>
#include "config.h"
#include "ranking.h"
#include "fixed_level.h"
#include "random_archive.h"
#include "game.h"
#include "utils.h"

/**
 * Main entry point of the game
 * 
 * What it does: Initializes all systems (ranking, progress, archive), handles user login,
 *               game mode selection, runs the appropriate game session, and manages
 *               persistence of scores and progress.
 * Inputs: None (command line arguments are ignored)
 * Output: Program exit code (0 = normal termination)
 */
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
            // Random Challenge Mode
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
            // Fixed Level Mode
            bool chooseAnotherFixedLevel = true;

            while (chooseAnotherFixedLevel) {
                int savedLevel = fixedProgress.getLastFixedLevel(username);
                int fixedLevelId = inputFixedLevel(savedLevel);
                FixedLevelConfig fixedConfig = getFixedLevelConfig(fixedLevelId);

                fixedProgress.setLastFixedLevel(username, fixedLevelId);
                fixedProgress.saveToFile(FIXED_PROGRESS_FILE);

                std::cout << "\nSelected Fixed Level " << fixedConfig.fixedLevelId
                          << " | Difficulty " << fixedConfig.difficulty
                          << " | Terrain layers: " << fixedConfig.terrainLayers;
                if (fixedConfig.difficulty == 1) std::cout << " | Wider beginner openings";
                std::cout << "\n";
                std::cout << "After you complete a fixed level, the game will automatically enter the next fixed level.\n";
                waitForEnter();

                bool currentCompleted = false;
                int currentEarnedScore = 0;
                bool requestedOtherLevel = false;
                int currentFixedProgress = fixedLevelId;

                {
                    Game game(fixedConfig);
                    game.run();
                    currentCompleted = game.isGameCompleted();
                    currentEarnedScore = game.getSessionScore();
                    requestedOtherLevel = game.wantsOtherFixedLevel();
                    currentFixedProgress = game.getCurrentFixedLevelId();
                    game.restoreTerminalForMenu();
                }

                earnedScore += currentEarnedScore;
                completed = completed || currentCompleted;

                if (currentFixedProgress >= 1 && currentFixedProgress <= FIXED_LEVEL_COUNT) {
                    fixedProgress.setLastFixedLevel(username, currentFixedProgress);
                    fixedProgress.saveToFile(FIXED_PROGRESS_FILE);
                }

                chooseAnotherFixedLevel = requestedOtherLevel;
            }
        } else {
            // Replay Random Level from Archive
            bool chooseAnotherRecord = true;

            while (chooseAnotherRecord) {
                std::vector<RandomLevelRecord> records = randomArchive.getUserRecords(username);
                int reloadMethod = inputRandomReloadMethod(records);
                if (reloadMethod == 0) break;

                RecordedRandomConfig config;
                bool configReady = false;

                if (reloadMethod == 1) {
                    // Reload by Record ID
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
                    // Reload by Seed
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
