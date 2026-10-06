# Connect Four with an Unbeatable AI

A Connect Four game built in C++ with SDL3, featuring an AI opponent that plays perfectly. When the AI moves first, it is mathematically guaranteed to win: no matter what you play, it will find the winning line.

## Why the AI Can't Lose

Connect Four is a **solved game**. In 1988, James D. Allen and Victor Allis independently proved that the first player can always force a win by starting in the center column. This AI doesn't rely on heuristics or guesswork. It searches the game tree to find the exact outcome of every possible move, so it always picks a move that leads to a forced win.

In testing, the AI won 80 out of 80 games, including games against an opponent using the same solver for its defense. Its slowest move across all games took 0.36 seconds.

## Features

- **Perfect play.** The AI computes the exact game-theoretic value of every position instead of estimating.
- **Fast responses.** An opening book handles the expensive early game, and live search covers the rest in well under a second.
- **Responsive window.** The AI thinks on a background thread, so the game never freezes.
- **Mouse controls.** The column under your cursor is highlighted on your turn.
- **Status in the title bar.** It shows whose turn it is, when the AI is thinking, and the result.
- **Win and draw detection**, with instant restart.

## Controls

| Input | Action |
|---|---|
| Left click | Drop a piece in the hovered column |
| R | Restart the game |
| Close window | Quit |

The AI plays **red** and moves first. You play **yellow**.

## Building

### Requirements

- A C++17 compiler (MSVC, GCC, or Clang)
- [SDL3](https://github.com/libsdl-org/SDL)

### Visual Studio

1. Create a new C++ project and add `WindowManager.cpp`, `Connect4Solver.h`, and `Connect4Book.h`.
2. Add the SDL3 `include` directory under **Project Properties → C/C++ → General → Additional Include Directories**.
3. Add the SDL3 `lib` directory under **Linker → General → Additional Library Directories**, and add `SDL3.lib` under **Linker → Input → Additional Dependencies**.
4. Set the C++ language standard to C++17 or later.
5. Copy `SDL3.dll` next to the built executable.
6. **Build in Release mode.** Debug builds can make the AI 10–50× slower.

### Command line (GCC / Clang)

```bash
g++ -O2 -std=c++17 WindowManager.cpp -o connect4 $(pkg-config --cflags --libs sdl3)
./connect4
```

### CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(ConnectFour CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(SDL3 REQUIRED)

add_executable(connect4 WindowManager.cpp)
target_link_libraries(connect4 PRIVATE SDL3::SDL3)
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Project Structure

```
├── WindowManager.cpp   # SDL3 window, rendering, input, and game loop
├── Connect4Solver.h    # Perfect-play solver (bitboards + alpha-beta search)
└── Connect4Book.h      # Precomputed opening book (1,033 positions)
```

## How the AI Works

The solver combines several standard techniques for solving Connect Four.

**Bitboards.** The board is stored as two 64-bit integers: one for the current player's pieces, one for all pieces. Each column takes 7 bits (6 rows plus a sentinel bit). Checking for four in a row then takes only a handful of bit shifts and AND operations, which makes searching millions of positions per second possible.

**Negamax with alpha-beta pruning.** The AI searches the game tree and skips branches that provably can't affect the result. Moves are tried center-first, and moves that create the most new threats are explored first, which makes pruning much more effective.

**Null-window search.** Rather than computing exact scores everywhere, the AI mostly asks yes/no questions such as "does this move win?" These are far cheaper to answer than a full evaluation.

**Transposition table.** Many move orders reach the same position. A table of about 85 MB caches positions that have already been analyzed, so they're never searched twice.

**Losing-move filtering.** Moves that let the opponent win immediately are discarded before searching, and forced blocks are played automatically.

**Opening book.** Early positions are the most expensive to solve; a single move could take up to a minute. These were solved ahead of time for every position the AI can face during its first five moves, and the answers are stored in `Connect4Book.h` for instant lookup. After that point, live search takes well under a second.

### Move selection

On each turn the AI picks, in order of preference:

1. A move that wins immediately
2. Any move that leads to a forced win
3. A move that holds a draw
4. If losing, the move that delays defeat the longest, in case the opponent makes a mistake

## Configuration

To let the human move first, change this line in `WindowManager.cpp`:

```cpp
const int AI_PLAYER = PLAYER2;
```

The AI will still play perfectly and will win the moment you make a mistake. However, a flawless first player can beat it, since that's what the math says. The opening book only covers games where the AI moves first, so its early moves will also be slower in this mode.

## Credits

The solving approach follows techniques described by [John Tromp](https://tromp.github.io/c4/c4.html) and Pascal Pons's [Connect 4 solver tutorial](http://blog.gamesolver.org/).