// ranking.cpp
// Ranking system implementation
#include "ranking.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>

// ========== Static Utility Functions ==========

/**
 * Trim whitespace from string
 * 
 * What it does: Removes leading and trailing whitespace.
 * Inputs: text - Input string
 * Output: Trimmed string
 */
std::string RankingSystem::trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

/**
 * Check if a line contains only a plain integer
 * 
 * What it does: Determines if the string represents a valid integer (after trimming).
 * Inputs: text - Input string
 * Output: true if it's a plain integer line, false otherwise
 */
bool RankingSystem::isPlainIntegerLine(const std::string& text) {
    std::string t = trim(text);
    if (t.empty()) return false;
    size_t pos = 0;
    if (t[0] == '+' || t[0] == '-') pos = 1;
    if (pos >= t.size()) return false;
    for (; pos < t.size(); pos++) {
        if (!std::isdigit(static_cast<unsigned char>(t[pos]))) return false;
    }
    return true;
}

/**
 * Clean username by removing ANSI escape sequences and control characters
 * 
 * What it does: Filters out terminal escape codes and keeps printable characters.
 * Inputs: rawName - Raw username string
 * Output: Cleaned username
 */
std::string RankingSystem::cleanUsername(const std::string& rawName) {
    std::string cleaned;
    int escapeState = 0; // 0 normal, 1 after ESC, 2 inside ESC[... sequence

    for (size_t i = 0; i < rawName.size(); i++) {
        unsigned char ch = static_cast<unsigned char>(rawName[i]);

        if (escapeState == 1) {
            if (ch == '[') {
                escapeState = 2;
            } else {
                escapeState = 0;
            }
            continue;
        }

        if (escapeState == 2) {
            // CSI parameters end at a final byte in the range @ to ~.
            if (ch >= 64 && ch <= 126) escapeState = 0;
            continue;
        }

        if (ch == 27) { // ESC
            escapeState = 1;
            continue;
        }

        // Keep normal printable ASCII and UTF-8 bytes, drop other controls.
        if ((ch >= 32 && ch != 127) || ch >= 128) {
            cleaned.push_back(static_cast<char>(ch));
        }
    }

    return trim(cleaned);
}

/**
 * Check if username is valid
 * 
 * What it does: Validates that the username contains at least one alphanumeric,
 *               underscore, hyphen or UTF-8 character.
 * Inputs: username - Username to validate
 * Output: true if valid, false otherwise
 */
bool RankingSystem::isValidUsername(const std::string& username) {
    if (username.empty()) return false;

    bool hasNameChar = false;
    for (unsigned char ch : username) {
        if (std::isalnum(ch) || ch == '_' || ch == '-' || ch >= 128) {
            hasNameChar = true;
            break;
        }
    }
    return hasNameChar;
}

/**
 * Parse a record line from file
 * 
 * What it does: Parses a line containing username and score.
 * Inputs: rawLine  - Raw line from file
 *         username - Output parameter for username
 *         score    - Output parameter for score
 * Output: true if parsing successful, false otherwise
 */
bool RankingSystem::parseRecordLine(const std::string& rawLine,
                                    std::string& username, int& score) {
    std::string line = trim(rawLine);
    if (line.empty()) return false;

    size_t splitPos = line.find_last_of('\t');
    if (splitPos == std::string::npos) {
        size_t commaPos = line.find_last_of(',');
        if (commaPos != std::string::npos) splitPos = commaPos;
        else splitPos = line.find_last_of(' ');
    }

    if (splitPos == std::string::npos) return false;

    username = cleanUsername(line.substr(0, splitPos));
    std::string scoreStr = trim(line.substr(splitPos + 1));
    if (!isValidUsername(username) || scoreStr.empty()) return false;

    try {
        size_t used = 0;
        long long parsedScore = std::stoll(scoreStr, &used);
        if (used != scoreStr.size()) return false;
        if (parsedScore < 0 || parsedScore > std::numeric_limits<int>::max()) return false;
        score = static_cast<int>(parsedScore);
    } catch (...) {
        return false;
    }

    return true;
}

// ========== RankingSystem Member Functions ==========

RankingSystem::RankingSystem() : records(nullptr), count(0), capacity(0) {}

RankingSystem::~RankingSystem() {
    delete[] records;
}

/**
 * Expand internal array if needed
 * 
 * What it does: Doubles the capacity when the array is full.
 * Inputs: None
 * Output: None
 */
void RankingSystem::expandIfNeeded() {
    if (count < capacity) return;

    int newCapacity = (capacity == 0 ? 4 : capacity * 2);
    PlayerRecord* newRecords = new PlayerRecord[newCapacity];

    for (int i = 0; i < count; i++) {
        newRecords[i] = records[i];
    }

    delete[] records;
    records = newRecords;
    capacity = newCapacity;
}

/**
 * Add or merge a record (keep highest score for same name)
 * 
 * What it does: Adds a new record or updates existing one with higher score.
 * Inputs: rawName - Raw username
 *         score   - Score to add/merge
 * Output: None
 */
void RankingSystem::addOrMergeRecord(const std::string& rawName, int score) {
    std::string name = cleanUsername(rawName);
    if (!isValidUsername(name) || score < 0) return;

    int existingIdx = findUser(name);
    if (existingIdx != -1) {
        records[existingIdx].score = std::max(records[existingIdx].score, score);
        return;
    }

    expandIfNeeded();
    records[count].username = name;
    records[count].score = score;
    count++;
}

/**
 * Find user record index by name
 * 
 * What it does: Searches for a user in the records.
 * Inputs: name - Username to search for
 * Output: Index of the user, or -1 if not found
 */
int RankingSystem::findUser(const std::string& name) const {
    std::string cleanedName = cleanUsername(name);
    for (int i = 0; i < count; i++) {
        if (records[i].username == cleanedName) return i;
    }
    return -1;
}

/**
 * Login or register a user
 * 
 * What it does: Returns the index of an existing user or creates a new one.
 *               If the name is invalid, uses "Player".
 * Inputs: name - Username to login or register
 * Output: Index of the user record
 */
int RankingSystem::loginOrRegister(const std::string& name) {
    std::string cleanedName = cleanUsername(name);
    if (!isValidUsername(cleanedName)) cleanedName = "Player";

    int idx = findUser(cleanedName);
    if (idx != -1) return idx;

    expandIfNeeded();
    records[count].username = cleanedName;
    records[count].score = 0;
    count++;
    return count - 1;
}

/**
 * Add score to a user
 * 
 * What it does: Increases the score of the user at the given index.
 * Inputs: idx   - User index
 *         delta - Score to add (can be negative)
 * Output: None
 */
void RankingSystem::addScore(int idx, int delta) {
    if (idx >= 0 && idx < count) {
        records[idx].score += delta;
        if (records[idx].score < 0) records[idx].score = 0;
    }
}

/**
 * Get top three players
 * 
 * What it does: Returns the top 3 players sorted by score descending,
 *               then by username ascending for ties.
 * Inputs: None
 * Output: Vector containing up to 3 PlayerRecord entries
 */
std::vector<PlayerRecord> RankingSystem::getTopThree() const {
    std::vector<PlayerRecord> sorted;
    for (int i = 0; i < count; i++) {
        if (isValidUsername(records[i].username) && records[i].score > 0) {
            sorted.push_back(records[i]);
        }
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const PlayerRecord& a, const PlayerRecord& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.username < b.username;
              });

    if (sorted.size() > 3) sorted.resize(3);
    return sorted;
}

/**
 * Get score of a user
 * 
 * What it does: Returns the current score of the user at the given index.
 * Inputs: idx - User index
 * Output: Current score (0 if index invalid)
 */
int RankingSystem::getScore(int idx) const {
    if (idx >= 0 && idx < count) return records[idx].score;
    return 0;
}

/**
 * Get username of a user
 * 
 * What it does: Returns the username at the given index.
 * Inputs: idx - User index
 * Output: Username string (empty if index invalid)
 */
std::string RankingSystem::getUsername(int idx) const {
    if (idx >= 0 && idx < count) return records[idx].username;
    return "";
}

/**
 * Load ranking data from file
 * 
 * What it does: Loads player records from a file.
 * Inputs: filename - Path to the ranking data file
 * Output: true if successful, false otherwise
 */
bool RankingSystem::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;

    delete[] records;
    records = nullptr;
    count = 0;
    capacity = 0;

    std::string firstLine;
    if (!std::getline(in, firstLine)) return false;

    bool firstLineIsCount = isPlainIntegerLine(firstLine);
    if (!firstLineIsCount) {
        std::string name;
        int sc = 0;
        if (parseRecordLine(firstLine, name, sc)) addOrMergeRecord(name, sc);
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string name;
        int sc = 0;
        if (parseRecordLine(line, name, sc)) {
            addOrMergeRecord(name, sc);
        }
    }

    return true;
}

/**
 * Save ranking data to file
 * 
 * What it does: Saves current ranking data to a file.
 * Inputs: filename - Path to the ranking data file
 * Output: true if successful, false otherwise
 */
bool RankingSystem::saveToFile(const std::string& filename) const {
    std::vector<PlayerRecord> cleanRecords;
    for (int i = 0; i < count; i++) {
        if (isValidUsername(records[i].username) && records[i].score > 0) {
            cleanRecords.push_back(records[i]);
        }
    }

    std::sort(cleanRecords.begin(), cleanRecords.end(),
              [](const PlayerRecord& a, const PlayerRecord& b) {
                  if (a.username != b.username) return a.username < b.username;
                  return a.score > b.score;
              });

    std::ofstream out(filename, std::ios::trunc);
    if (!out.is_open()) return false;

    out << cleanRecords.size() << "\n";
    for (const auto& record : cleanRecords) {
        out << record.username << "\t" << record.score << "\n";
    }
    return true;
}
