# Frozen Spark
Welcome to **Frozen Spark**, a terminal-based puzzle-platformer. Control two elemental characters through treacherous, procedurally generated or hand-crafted levels using only your terminal. **Don't laugh — you won't make it past the second level either**.
## Team Members
Wu Yingxi 3036662452;
Xu Jingtong 3036669412;
Deng Yuan 3036589973;
Liu Jiahe 3036292635
## 🎮 Game Description
In Frozen Spark, you manage two heroes: **FireBoy** and **IceGirl**. Your objective is to navigate both characters to the exit (★) while avoiding elemental hazards.
- **FireBoy** can cross **Lava** but dies in **Water**.
- **IceGirl** can cross **Water** but dies in **Lava**.
- Both must avoid **Spikes** and cooperate to trigger **Switches** that open **Doors** blocking the path.
## 🚀 Features & Implementation
The game is built to meet specific technical requirements, integrating core C++ concepts into the gameplay experience:
### 1. Generation of Random Events
- **Implementation**: The game features a Procedural Level Generator.
- **How it works**: In "Random Challenge Mode," the game uses ```std::mt19937``` and time-based seeds to generate unique maps every time. It randomly decides the placement of terrain layers, the frequency of hazards (lava/water), and whether a level requires cooperative switch-puzzles. 
### 2. Data Structures for Storing Data
-	**Implementation**: Extensive use of ```std::vector```, ```struct```, and ```class``` hierarchies. 
-	**How it works**:
  o	```std::vector<std::vector<TileType>>``` stores the 2D grid of the map. 
  o	```struct Player``` and ```struct Switch``` store entity states like position, active status, and link connections. 
  o	```std::vector<RandomLevelRecord>``` manages archives of played levels. 
### 3. Dynamic Memory Management
-	**Implementation**: Manual memory management in the ```RankingSystem``` class. 
-	**How it works**: The ranking system uses a dynamic array (```PlayerRecord* records```) rather than a standard container for its core storage. It utilizes ```new[]``` to allocate memory and ```delete[]``` to free it during expansion or destruction, ensuring efficient memory usage for the leaderboard. 
### 4. File Input/Output
-	**Implementation**: Persistent storage for rankings, progress, and level seeds. 
-	**How it works**:
  o	```ranking_data.txt```: Saves player usernames and total scores. 
  o	```fixed_progress_data.txt```: Tracks the furthest fixed level reached by each user. 
  o	```random_level_records.txt```: Stores seeds of previously generated random levels so they can be replayed later. 
### 5. Program Codes in Multiple Files
-	**Implementation**: The project is modularized into distinct header and source files. 
-	**How it works**: Logic is separated into ```main.cpp``` (entry/menus), ```game.cpp``` (engine/rendering), ```ranking.cpp``` (leaderboard), ```fixed_level.cpp``` (stage configs), and ```terminal.cpp``` (input handling). 
### 6. Multiple Difficulty Levels
-	**Implementation**: Three selectable difficulties: Beginner, Intermediate, and Advanced. 
-	**How it works**: Difficulty affects the number of terrain layers (3, 5, or 7), the width of gaps for safer falling, and the points rewarded upon completion. 
### 7. Physics-Based Gravity System
Our game implements a real-time gravity mechanic to simulate a platformer experience within the constraints of a terminal window. 
-	**Time-Based Execution**: Gravity does not rely on frame rate; instead, it is governed by a timer in the main game loop that triggers every ```FALL_INTERVAL_US``` microseconds, as defined in ```config.h```. 
-	**Collision Detection**: The ```applyGravityToPlayer``` function in ```game.cpp``` constantly checks the ```TileType``` directly beneath each character. 
-	**Dynamic Falling**: Characters will descend if the tile below is ```EMPTY``` or a liquid hazard, but they will come to a halt if they land on a ```WALL``` or a ```DOOR_CLOSED```. 
-	**Integrated Hazard Logic**: Falling is not just about movement; if a character falls into a lethal tile (such as IceGirl falling into ```LAVA```), the ```isHazardForPlayer``` check immediately triggers a ```gameOver``` state. 
-	**Cooperative Interaction**: The gravity system interacts with the switch mechanic—if a character is standing on a ```SWITCH``` and moves or falls off it, the linked ```DOOR_CLOSED``` will instantly snap shut, potentially trapping or crushing the other player. 
## 🛠 Libraries Used
This project relies primarily on the **C++ Standard Library**. However, it utilizes **POSIX-specific system headers** for terminal control:
-	```<unistd.h>``` and ```<termios.h>```: Used within ```terminal.h/cpp``` and ```game.cpp``` to enable "Raw Mode". This allows the game to capture keypresses instantly without waiting for the "Enter" key and to hide the terminal cursor for a cleaner UI. 
-	**ANSI Escape Codes**: Integrated directly into string streams to provide colored graphics and screen clearing without external graphical libraries. 
## ⚡ Quick Start
**Compilation**

The project includes a ```Makefile``` for easy compilation. Open your terminal in the project directory and run:
```Bash```
```make```
This will produce the executable **Frozen Spark**.

**Execution**

To launch the game:
```Bash```
```./Frozen_Spark```

**Controls**

- **WASD / Arrow Keys**: Move the active character.
- **Tab**: Switch control between FireBoy and IceGirl.
- **R**: Restart the current level.
- **F**: Generate a brand new random map (Random Mode only).
- **Q**: Quit to menu.
