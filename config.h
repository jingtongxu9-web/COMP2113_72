// config.h
// Global game configuration constants
#ifndef CONFIG_H
#define CONFIG_H

// Map dimensions
constexpr int WIDTH  = 60;
constexpr int HEIGHT = 25;

// Maximum number of levels in random challenge mode
constexpr int MAX_LEVELS = 20;

// Filenames for various persistent data
constexpr const char* RANKING_FILE            = "ranking_data.txt";
constexpr const char* FIXED_PROGRESS_FILE     = "fixed_progress_data.txt";
constexpr const char* RANDOM_LEVEL_RECORD_FILE = "random_level_records.txt";

// Gravity fall interval (microseconds), determines falling speed
constexpr int FALL_INTERVAL_US = 200000;

#endif // CONFIG_H
