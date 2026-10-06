#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // Required header for entry points in SDL3
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#include "Connect4Solver.h"

const int ROWS = 6;
const int COLS = 7;
const int CELL_SIZE = 100;
const int PADDING = 10;
const int SCREEN_WIDTH = COLS * CELL_SIZE;
const int SCREEN_HEIGHT = ROWS * CELL_SIZE;

enum Player { EMPTY = 0, PLAYER1 = 1, PLAYER2 = 2 };

// ---------------------------------------------------------------------------
// GAME OPTIONS
//
// Connect 4 is solved: the FIRST player can always force a win.
//
//   Normal mode (easyMode = false): the AI moves first and is guaranteed to win.
//   Easy mode   (easyMode = true):  YOU move first. With perfect play you can
//                                   win, but the AI punishes any mistake.
//
// You can also toggle this in-game with the E key (starts a new game).
// ---------------------------------------------------------------------------
bool easyMode = false;

// Per-move thinking budget for the AI in easy mode, in seconds. Early in the
// game, if the AI can't finish its search in time it plays a strong heuristic
// move instead; by its 4th move it searches to the end every time.
// Normal mode has no limit (it doesn't need one thanks to the opening book).
const double EASY_MODE_TIME_LIMIT = 3.0;

// Whoever moves first plays red (PLAYER1). Set by resetGame().
int AI_PLAYER = PLAYER1;
int HUMAN_PLAYER = PLAYER2;

int board[ROWS][COLS] = { EMPTY };
int currentPlayer = PLAYER1;
bool gameOver = false;

c4::Position position;   // bitboard mirror of 'board' used by the AI
c4::Solver solver;       // ~85 MB transposition table, reused between moves

// Custom function to draw a filled circle pixel-by-pixel (SDL3 native is rectangular)
void DrawFilledCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius) {
    for (float w = 0; w < radius * 2; w++) {
        for (float h = 0; h < radius * 2; h++) {
            float dx = radius - w;
            float dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderPoint(renderer, centerX + dx, centerY + dy);
            }
        }
    }
}

// Logic to apply piece gravity inside a chosen column
bool makeMove(int col) {
    if (col < 0 || col >= COLS) return false;
    for (int r = ROWS - 1; r >= 0; r--) {
        if (board[r][col] == EMPTY) {
            board[r][col] = currentPlayer;
            position.playCol(col);   // keep the AI's view in sync
            return true;
        }
    }
    return false;
}

// Scans grid array to determine if current player achieved 4-in-a-row
bool checkWin() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (board[r][c] != currentPlayer) continue;

            if (c + 3 < COLS && board[r][c + 1] == currentPlayer && board[r][c + 2] == currentPlayer && board[r][c + 3] == currentPlayer) return true;
            if (r + 3 < ROWS && board[r + 1][c] == currentPlayer && board[r + 2][c] == currentPlayer && board[r + 3][c] == currentPlayer) return true;
            if (r + 3 < ROWS && c + 3 < COLS && board[r + 1][c + 1] == currentPlayer && board[r + 2][c + 2] == currentPlayer && board[r + 3][c + 3] == currentPlayer) return true;
            if (r - 3 >= 0 && c + 3 < COLS && board[r - 1][c + 1] == currentPlayer && board[r - 2][c + 2] == currentPlayer && board[r - 3][c + 3] == currentPlayer) return true;
        }
    }
    return false;
}

void updateTitle(SDL_Window* window, const char* status) {
    std::string title = std::string("SDL3 Connect Four [") + (easyMode ? "Easy" : "Normal") +
        " - you are " + (HUMAN_PLAYER == PLAYER1 ? "Red" : "Yellow") + "] - " + status;
    SDL_SetWindowTitle(window, title.c_str());
}

// Plays a move for currentPlayer and handles win / draw / turn switching.
void applyMove(SDL_Window* window, int col) {
    if (!makeMove(col)) return;

    if (checkWin()) {
        gameOver = true;
        if (currentPlayer == AI_PLAYER) {
            std::cout << "AI wins! Press R to play again.\n";
            updateTitle(window, "AI wins! Press R to play again");
        }
        else {
            std::cout << "You win! Press R to play again.\n";
            updateTitle(window, "You win! Press R to play again");
        }
    }
    else if (position.movesPlayed() == ROWS * COLS) {
        gameOver = true;
        std::cout << "Draw! Press R to play again.\n";
        updateTitle(window, "Draw! Press R to play again");
    }
    else {
        currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;
        updateTitle(window, currentPlayer == AI_PLAYER ? "AI thinking..." : "Your turn");
    }
}

void resetGame(SDL_Window* window) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            board[r][c] = EMPTY;
    position = c4::Position();
    AI_PLAYER = easyMode ? PLAYER2 : PLAYER1;
    HUMAN_PLAYER = easyMode ? PLAYER1 : PLAYER2;
    currentPlayer = PLAYER1;
    gameOver = false;
    updateTitle(window, currentPlayer == AI_PLAYER ? "AI thinking..." : "Your turn");
}

// Renders the board view using the SDL3 subsystem
void renderBoard(SDL_Renderer* renderer, int hoveredCol) {
    // SDL3 handles colors as floats or uint8 mapping. Clear window with White.
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    float radius = (CELL_SIZE / 2.0f) - PADDING;
    bool humanTurn = (currentPlayer == HUMAN_PLAYER);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            // 1. Determine background square color (highlight hovered column on the human's turn)
            if (c == hoveredCol && !gameOver && humanTurn) {
                SDL_SetRenderDrawColor(renderer, 41, 128, 185, 255); // Highlight Blue
            }
            else {
                SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255); // Classic Board Blue
            }

            // SDL3 uses SDL_FRect (floating point rectangles) for core drawing
            SDL_FRect cellRect = { static_cast<float>(c * CELL_SIZE), static_cast<float>(r * CELL_SIZE), CELL_SIZE, CELL_SIZE };
            SDL_RenderFillRect(renderer, &cellRect);

            // 2. Establish local centers for the visual circles
            float centerX = c * CELL_SIZE + (CELL_SIZE / 2.0f);
            float centerY = r * CELL_SIZE + (CELL_SIZE / 2.0f);

            // 3. Render appropriate slots
            if (board[r][c] == EMPTY) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White mask
            }
            else if (board[r][c] == PLAYER1) {
                SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);  // Red 
            }
            else if (board[r][c] == PLAYER2) {
                SDL_SetRenderDrawColor(renderer, 241, 196, 15, 255);  // Yellow
            }
            DrawFilledCircle(renderer, centerX, centerY, radius);
        }
    }
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    // SDL3 initialization returns a boolean state (true = success)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL3 Init Failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // SDL3 groups window and renderer setup into a clean, single call
    if (!SDL_CreateWindowAndRenderer("SDL3 Connect Four", SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer)) {
        std::cerr << "Failed to create SDL3 Window/Renderer: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    std::cout << "Connect Four: click a column to drop a piece.\n"
        << "R = restart, E = toggle easy mode (you move first)\n";
    resetGame(window);

    bool quit = false;
    SDL_Event e;
    int hoveredCol = -1;

    // The AI searches on a background thread so the window stays responsive.
    std::future<int> aiFuture;
    bool aiThinking = false;
    auto aiStart = std::chrono::steady_clock::now();

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            // SDL3 renames event macros from SDL_QUIT to SDL_EVENT_QUIT
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            // Tracks mouse movement events
            else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                hoveredCol = static_cast<int>(e.motion.x) / CELL_SIZE;
            }
            else if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
                hoveredCol = -1;
            }
            // Human move: only on the human's turn and while the AI is idle
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !gameOver &&
                currentPlayer == HUMAN_PLAYER && !aiThinking) {
                applyMove(window, static_cast<int>(e.button.x) / CELL_SIZE);
            }
            // R restarts (not while the AI is mid-search)
            else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R && !aiThinking) {
                resetGame(window);
            }
            // E toggles easy mode and starts a new game
            else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_E && !aiThinking) {
                easyMode = !easyMode;
                std::cout << (easyMode ? "Easy mode: you move first. Good luck!\n"
                    : "Normal mode: the AI moves first.\n");
                resetGame(window);
            }
        }

        // Start an AI search when it's the AI's turn
        if (!gameOver && currentPlayer == AI_PLAYER && !aiThinking) {
            aiThinking = true;
            aiStart = std::chrono::steady_clock::now();
            c4::Position snapshot = position;
            double timeLimit = easyMode ? EASY_MODE_TIME_LIMIT : 0.0;   // 0 = unlimited
            aiFuture = std::async(std::launch::async, [snapshot, timeLimit]() {
                return solver.bestMove(snapshot, timeLimit);
                });
        }

        // Collect the AI's move once the search finishes
        if (aiThinking && aiFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            int col = aiFuture.get();
            aiThinking = false;
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - aiStart).count();
            std::cout << "AI plays column " << (col + 1) << " (" << secs << "s"
                << (solver.lastMoveTimedOut() ? ", out of time - heuristic move" : "") << ")\n";
            applyMove(window, col);
        }

        renderBoard(renderer, hoveredCol);
        SDL_Delay(16); // Limits thread frame-time roughly to ~60 FPS
    }

    // Stop any running search before tearing down
    if (aiThinking) {
        solver.abort = true;
        aiFuture.wait();
    }

    // Clean resource handling
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}