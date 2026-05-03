// ranking.h
// Ranking system: player score recording, ranking display, and file persistence
#ifndef RANKING_H
#define RANKING_H

#include <string>
#include <vector>

// Simple player record structure
struct PlayerRecord {
    std::string username;
    int score;
};

// Ranking system based on dynamic array
class RankingSystem {
public:
    RankingSystem();
    ~RankingSystem();

    /**
     * Find user record index by name
     * 
     * What it does: Searches for a user in the records.
     * Inputs: name - Username to search for
     * Output: Index of the user, or -1 if not found
     */
    int findUser(const std::string& name) const;

    /**
     * Login or register a user
     * 
     * What it does: Returns the index of an existing user or creates a new one.
     *               If the name is invalid, uses "Player".
     * Inputs: name - Username to login or register
     * Output: Index of the user record
     */
    int loginOrRegister(const std::string& name);

    /**
     * Add score to a user
     * 
     * What it does: Increases the score of the user at the given index.
     * Inputs: idx   - User index
     *         delta - Score to add (can be negative)
     * Output: None
     */
    void addScore(int idx, int delta);

    /**
     * Get top three players
     * 
     * What it does: Returns the top 3 players sorted by score descending,
     *               then by username ascending for ties.
     * Inputs: None
     * Output: Vector containing up to 3 PlayerRecord entries
     */
    std::vector<PlayerRecord> getTopThree() const;

    /**
     * Get score of a user
     * 
     * What it does: Returns the current score of the user at the given index.
     * Inputs: idx - User index
     * Output: Current score (0 if index invalid)
     */
    int getScore(int idx) const;

    /**
     * Get username of a user
     * 
     * What it does: Returns the username at the given index.
     * Inputs: idx - User index
     * Output: Username string (empty if index invalid)
     */
    std::string getUsername(int idx) const;

    /**
     * Load ranking data from file
     * 
     * What it does: Loads player records from a file.
     * Inputs: filename - Path to the ranking data file
     * Output: true if successful, false otherwise
     */
    bool loadFromFile(const std::string& filename);

    /**
     * Save ranking data to file
     * 
     * What it does: Saves current ranking data to a file.
     * Inputs: filename - Path to the ranking data file
     * Output: true if successful, false otherwise
     */
    bool saveToFile(const std::string& filename) const;

private:
    PlayerRecord* records;   // Dynamic array
    int count;               // Current number of records
    int capacity;            // Current capacity

    /**
     * Expand internal array if needed
     * 
     * What it does: Doubles the capacity when the array is full.
     * Inputs: None
     * Output: None
     */
    void expandIfNeeded();

    /**
     * Add or merge a record (keep highest score for same name)
     * 
     * What it does: Adds a new record or updates existing one with higher score.
     * Inputs: rawName - Raw username
     *         score   - Score to add/merge
     * Output: None
     */
    void addOrMergeRecord(const std::string& rawName, int score);

    // Static helper functions for string and record parsing
    static std::string trim(const std::string& text);
    static bool isPlainIntegerLine(const std::string& text);
    static std::string cleanUsername(const std::string& rawName);
    static bool isValidUsername(const std::string& username);
    static bool parseRecordLine(const std::string& rawLine,
                                std::string& username, int& score);
};

#endif // RANKING_H
