// terminal.cpp
// Terminal control class implementation
#include "terminal.h"
#include <iostream>
#include <sys/select.h>
#include <unistd.h>

/**
 * Enable raw mode (no echo, character-by-character input)
 * 
 * What it does: Switches the terminal to raw mode for direct keyboard input
 *               without line buffering or echo.
 * Inputs: None
 * Output: None
 */
void Terminal::enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode = true;
}

/**
 * Restore original terminal settings
 * 
 * What it does: Restores the terminal to its original mode (cooked mode).
 * Inputs: None
 * Output: None
 */
void Terminal::disableRawMode() {
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode = false;
    }
}

/**
 * Non-blocking read of one key press
 * 
 * What it does: Attempts to read a single character without blocking.
 *               Returns 0 if no input is available.
 * Inputs: None
 * Output: Character read, or 0 if no key was pressed
 */
char Terminal::readKey() {
    char c = 0;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        ssize_t bytesRead=read(STDIN_FILENO, &c, 1);
	if (bytesRead !=1){
		c=0;
	}
    }
    return c;
}

/**
 * Move cursor to top-left corner of the screen
 * 
 * What it does: Moves the cursor to position (0,0).
 * Inputs: None
 * Output: None
 */
void Terminal::resetCursor() {
    std::cout << "\033[H";
}

/**
 * Clear screen and move cursor to top-left
 * 
 * What it does: Clears the entire screen and resets cursor position.
 * Inputs: None
 * Output: None
 */
void Terminal::clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

/**
 * Hide the cursor
 * 
 * What it does: Hides the terminal cursor using ANSI escape sequence.
 * Inputs: None
 * Output: None
 */
void Terminal::hideCursor() {
    std::cout << "\033[?25l" << std::flush;
}

/**
 * Show the cursor
 * 
 * What it does: Makes the terminal cursor visible again.
 * Inputs: None
 * Output: None
 */
void Terminal::showCursor() {
    std::cout << "\033[?25h" << std::flush;
}

/**
 * Generate 24-bit foreground color ANSI escape sequence
 * 
 * What it does: Creates an ANSI string for true color (RGB) text foreground.
 * Inputs: r, g, b - Red, Green, Blue values (0-255)
 * Output: ANSI escape sequence string
 */
std::string Terminal::getColorStr(int r, int g, int b) {
    return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
}

/**
 * Generate 24-bit background color ANSI escape sequence
 * 
 * What it does: Creates an ANSI string for true color (RGB) background.
 * Inputs: r, g, b - Red, Green, Blue values (0-255)
 * Output: ANSI escape sequence string
 */
std::string Terminal::getBgColorStr(int r, int g, int b) {
    return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
}

/**
 * Generate reset color/style sequence
 * 
 * What it does: Returns the ANSI sequence to reset all colors and styles.
 * Inputs: None
 * Output: ANSI reset sequence string
 */
std::string Terminal::getResetStr() {
    return "\033[0m";
}
