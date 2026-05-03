// terminal.h
// Terminal control class: raw mode, cursor hiding, color output, etc.
#ifndef TERMINAL_H
#define TERMINAL_H

#include <string>
#include <termios.h>
#include <unistd.h>

class Terminal {
public:
    /**
     * Enable raw mode (no echo, character-by-character input)
     * 
     * What it does: Switches the terminal to raw mode for direct keyboard input
     *               without line buffering or echo.
     * Inputs: None
     * Output: None
     */
    void enableRawMode();

    /**
     * Restore original terminal settings
     * 
     * What it does: Restores the terminal to its original mode (cooked mode).
     * Inputs: None
     * Output: None
     */
    void disableRawMode();

    /**
     * Non-blocking read of one key press
     * 
     * What it does: Attempts to read a single character without blocking.
     *               Returns 0 if no input is available.
     * Inputs: None
     * Output: Character read, or 0 if no key was pressed
     */
    char readKey();

    /**
     * Move cursor to top-left corner of the screen
     * 
     * What it does: Moves the cursor to position (0,0).
     * Inputs: None
     * Output: None
     */
    void resetCursor();

    /**
     * Clear screen and move cursor to top-left
     * 
     * What it does: Clears the entire screen and resets cursor position.
     * Inputs: None
     * Output: None
     */
    void clearScreen();

    /**
     * Hide the cursor
     * 
     * What it does: Hides the terminal cursor using ANSI escape sequence.
     * Inputs: None
     * Output: None
     */
    void hideCursor();

    /**
     * Show the cursor
     * 
     * What it does: Makes the terminal cursor visible again.
     * Inputs: None
     * Output: None
     */
    void showCursor();

    /**
     * Generate 24-bit foreground color ANSI escape sequence
     * 
     * What it does: Creates an ANSI string for true color (RGB) text foreground.
     * Inputs: r, g, b - Red, Green, Blue values (0-255)
     * Output: ANSI escape sequence string
     */
    std::string getColorStr(int r, int g, int b);

    /**
     * Generate 24-bit background color ANSI escape sequence
     * 
     * What it does: Creates an ANSI string for true color (RGB) background.
     * Inputs: r, g, b - Red, Green, Blue values (0-255)
     * Output: ANSI escape sequence string
     */
    std::string getBgColorStr(int r, int g, int b);

    /**
     * Generate reset color/style sequence
     * 
     * What it does: Returns the ANSI sequence to reset all colors and styles.
     * Inputs: None
     * Output: ANSI reset sequence string
     */
    std::string getResetStr();

private:
    struct termios orig_termios;
    bool raw_mode = false;
};

#endif // TERMINAL_H
