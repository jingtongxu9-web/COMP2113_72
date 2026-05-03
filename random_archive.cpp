// random_archive.cpp
// Implementation of the random level archive class
#include "random_archive.h"

RandomLevelArchive::RandomLevelArchive() : nextRecordId(1) {}

/**
 * Load records from file
 * 
 * What it does: Loads all previously saved random level records from a file.
 * Inputs: filename - Path to the archive file
 * Output: true if successful, false otherwise
 */
bool RandomLevelArchive::loadFromFile(const std::string& filename) {
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

/**
 * Save records to file
 * 
 * What it does: Saves all current records to a file (overwriting existing content).
 * Inputs: filename - Path to the archive file
 * Output: true if successful, false otherwise
 */
bool RandomLevelArchive::saveToFile(const std::string& filename) const {
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

/**
 * Add a new record
 * 
 * What it does: Creates and stores a new random level result record for a player.
 * Inputs: username - Player's username
 *         result   - The result of the played random level
 * Output: None
 */
void RandomLevelArchive::addRecord(const std::string& username, const RandomLevelResult& result) {
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

/**
 * Get all records for a specific user
 * 
 * What it does: Retrieves all random level records for a user, sorted by record ID descending (newest first).
 * Inputs: username - Player's username
 * Output: Vector of RandomLevelRecord (sorted newest first)
 */
std::vector<RandomLevelRecord> RandomLevelArchive::getUserRecords(const std::string& username) const {
    std::vector<RandomLevelRecord> userRecords;
    for (const auto& record : records) {
        if (record.username == username) userRecords.push_back(record);
    }

    std::sort(userRecords.begin(), userRecords.end(),
              [](const RandomLevelRecord& a, const RandomLevelRecord& b) {
                  return a.recordId > b.recordId;
              });

    return userRecords;
}

/**
 * Find a specific record by ID for a user
 * 
 * What it does: Searches for a record matching both username and recordId.
 * Inputs: username   - Player's username
 *         recordId   - Record identifier
 *         outRecord  - Output parameter to store the found record
 * Output: true if record found, false otherwise
 */
bool RandomLevelArchive::findRecordByIdForUser(const std::string& username, int recordId, RandomLevelRecord& outRecord) const {
    for (const auto& record : records) {
        if (record.username == username && record.recordId == recordId) {
            outRecord = record;
            return true;
        }
    }
    return false;
}

/**
 * Find the newest record by seed for a user
 * 
 * What it does: Finds the most recent record where the user played a level with the given seed.
 * Inputs: username - Player's username
 *         seed     - Random seed to search for
 *         outRecord - Output parameter to store the found record
 * Output: true if a matching record is found, false otherwise
 */
bool RandomLevelArchive::findNewestRecordBySeedForUser(const std::string& username, unsigned int seed, RandomLevelRecord& outRecord) const {
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
