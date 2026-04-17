# ImpossibleConnect4
ImpossibleConnect4

A hardware-accelerated 2D rendering engine and adversarial search agent designed to perfectly solve Connect Four, engineered for peak performance and strategic dominance.
The Pitch

We didn't rely on bloated game engines or pre-made logic. By interfacing directly with the GPU via Modern OpenGL and utilizing bit-level board representations, ImpossibleConnect4 achieves a "god-mode" state where the AI can simulate millions of future outcomes per second. It doesn't just play; it solves the board.
Architecture

    Low-Level Graphics Core: A custom C++ rendering pipeline that talks directly to the GPU. It uses GLSL shaders to draw the grid and animate pieces, bypassing the overhead of traditional game engines for raw frame-rate efficiency.

    Bitboard State Engine: Instead of heavy 2D arrays, the board is compressed into two 64-bit integers. This allows the engine to check for win conditions and valid moves using lightning-fast bitwise shifts and XOR operations in O(1) time.

    Recursive "God-Brain": An optimized Minimax algorithm that builds a deep decision tree of every possible future. It assigns a heuristic score to every board state to ensure the AI always picks the path to certain victory.

    Alpha-Beta Pruning Filter: A high-speed culling mechanism that "prunes" millions of suboptimal moves from the search tree. This allows the bot to "see" the endgame over 15 turns in advance in just a few milliseconds.

    Interactive UI Pipeline: A real-time interface driven by GLFW and GLM, translating raw mouse input into precise column placements and rendering the bot's calculations with zero input latency.

How to Run

    Clone this repository (100% pure C++ source code with zero third-party game engine bloat).

    Build the project using CMake (this will dynamically link the necessary OpenGL loaders and math libraries):
    Bash

    cmake -B build
    cmake --build build --config Release

    Watch the console as the engine initializes the OpenGL 4.6 context and compiles the vertex/fragment shaders!

    We recommend using vcpkg or pacman to ensure all C++ dependencies (GLFW, GLM, GLAD) are in your path.

    Launch the engine:
    Bash

    # On Windows:
    ./build/Release/ImpossibleConnect4.exe

    # On Linux:
    ./build/ImpossibleConnect4
