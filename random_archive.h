// random_archive.h
// Random level archive: stores and queries performance records of random challenge levels
#ifndef RANDOM_ARCHIVE_H
#define RANDOM_ARCHIVE_H

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

// Result of a single random challenge level run (used for internal recording)
struct RandomLevelResult {
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    int levelNumber;
    unsigned int seed;
    int scoreEarned;
    bool completed;
};

// Persistent storage record for a random level
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

// Core configuration needed to reload a recorded level
struct RecordedRandomConfig {
    int recordId;
    int difficulty;
    int terrainLayers;
    int rewardPoints;
    int levelNumber;
    unsigned int seed;
};

// Random level archive management system
class RandomLevelArchive {
private:
    std::vector<RandomLevelRecord> records;
    int nextRecordId;

public:
    RandomLevelArchive();

    /**
     * Load records from file
     * 
     * What it does: Loads all previously saved random level records from a file.
     * Inputs: filename - Path to the archive file
     * Output: true if successful, false otherwise
     */
    bool loadFromFile(const std::string& filename);

    /**
     * Save records to file
     * 
     * What it does: Saves all current records to a file (overwriting existing content).
     * Inputs: filename - Path to the archive file
     * Output: true if successful, false otherwise
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * Add a new record
     * 
     * What it does: Creates and stores a new random level result record for a player.
     * Inputs: username - Player's username
     *         result   - The result of the played random level
     * Output: None
     */
    void addRecord(const std::string& username, const RandomLevelResult& result);

    /**
     * Get all records for a specific user
     * 
     * What it does: Retrieves all random level records for a user, sorted by record ID descending (newest first).
     * Inputs: username - Player's username
     * Output: Vector of RandomLevelRecord (sorted newest first)
     */
    std::vector<RandomLevelRecord> getUserRecords(const std::string& username) const;

    /**
     * Find a specific record by ID for a user
     * 
     * What it does: Searches for a record matching both username and recordId.
     * Inputs: username   - Player's username
     *         recordId   - Record identifier
     *         outRecord  - Output parameter to store the found record
     * Output: true if record found, false otherwise
     */
    bool findRecordByIdForUser(const std::string& username, int recordId,
                               RandomLevelRecord& outRecord) const;

    /**
     * Find the newest record by seed for a user
     * 
     * What it does: Finds the most recent record where the user played a level with the given seed.
     * Inputs: username - Player's username
     *         seed     - Random seed to search for
     *         outRecord - Output parameter to store the found record
     * Output: true if a matching record is found, false otherwise
     */
    bool findNewestRecordBySeedForUser(const std::string& username, unsigned int seed,
                                       RandomLevelRecord& outRecord) const;
};

#endif // RANDOM_ARCHIVE_H
