#pragma once
// Perfect-play Connect 4 solver.
// Technique: bitboards + negamax with alpha-beta pruning + transposition table
// + move ordering + null-window search (the approach popularized by
// John Tromp and Pascal Pons). Score convention for the player to move:
//   > 0 : win  (bigger = wins sooner)
//   = 0 : draw
//   < 0 : loss (smaller = loses sooner)

#include <atomic>
#include <cstdint>
#include <vector>

#include "Connect4Book.h"

namespace c4 {

    constexpr int WIDTH = 7;
    constexpr int HEIGHT = 6;
    constexpr int MIN_SCORE = -(WIDTH * HEIGHT) / 2;
    constexpr int MAX_SCORE = (WIDTH * HEIGHT + 1) / 2;
    static_assert(WIDTH* (HEIGHT + 1) <= 64, "Board does not fit in a 64-bit bitboard");

    // Search columns center-first: center moves are statistically strongest,
    // which makes alpha-beta prune far more.
    constexpr int COLUMN_ORDER[WIDTH] = { 3, 2, 4, 1, 5, 0, 6 };

    constexpr uint64_t bottomRow(int w) {
        return w == 0 ? 0 : bottomRow(w - 1) | (1ULL << (w - 1) * (HEIGHT + 1));
    }

    inline int popcount(uint64_t m) {
        int c = 0;
        for (; m; c++) m &= m - 1;
        return c;
    }

    // Each column uses HEIGHT+1 bits (one spare bit on top as a sentinel).
    // 'current' = stones of the player to move, 'mask' = all stones.
    class Position {
    public:
        static constexpr uint64_t BOTTOM_MASK = bottomRow(WIDTH);
        static constexpr uint64_t BOARD_MASK = BOTTOM_MASK * ((1ULL << HEIGHT) - 1);

        static constexpr uint64_t topMask(int col) { return (1ULL << (HEIGHT - 1)) << col * (HEIGHT + 1); }
        static constexpr uint64_t bottomMask(int col) { return 1ULL << col * (HEIGHT + 1); }
        static constexpr uint64_t columnMask(int col) { return ((1ULL << HEIGHT) - 1) << col * (HEIGHT + 1); }

        bool canPlay(int col) const { return (mask & topMask(col)) == 0; }
        void playCol(int col) { play((mask + bottomMask(col)) & columnMask(col)); }
        void play(uint64_t move) { current ^= mask; mask |= move; moves++; }

        bool isWinningMove(int col) const { return winningPosition() & possible() & columnMask(col); }
        bool canWinNext() const { return winningPosition() & possible(); }
        int movesPlayed() const { return moves; }
        uint64_t key() const { return current + mask; }

        // Playable moves that don't let the opponent win immediately.
        uint64_t possibleNonLosingMoves() const {
            uint64_t possibleMask = possible();
            uint64_t opponentWin = opponentWinningPosition();
            uint64_t forced = possibleMask & opponentWin;
            if (forced) {
                if (forced & (forced - 1)) return 0;   // two threats: can't block both
                possibleMask = forced;                 // must block
            }
            return possibleMask & ~(opponentWin >> 1); // don't play right under their threat
        }

        // Heuristic for move ordering: how many winning spots a move creates.
        int moveScore(uint64_t move) const { return popcount(computeWinningPosition(current | move, mask)); }

    private:
        uint64_t current = 0, mask = 0;
        int moves = 0;

        uint64_t possible() const { return (mask + BOTTOM_MASK) & BOARD_MASK; }
        uint64_t winningPosition() const { return computeWinningPosition(current, mask); }
        uint64_t opponentWinningPosition() const { return computeWinningPosition(current ^ mask, mask); }

        // Empty cells that would complete 4-in-a-row for 'pos'.
        static uint64_t computeWinningPosition(uint64_t pos, uint64_t mask) {
            // vertical
            uint64_t r = (pos << 1) & (pos << 2) & (pos << 3);
            // horizontal and both diagonals: shift by H+1, H, H+2
            const int shifts[3] = { HEIGHT + 1, HEIGHT, HEIGHT + 2 };
            for (int s : shifts) {
                uint64_t p = (pos << s) & (pos << 2 * s);
                r |= p & (pos << 3 * s);
                r |= p & (pos >> s);
                p = (pos >> s) & (pos >> 2 * s);
                r |= p & (pos << s);
                r |= p & (pos >> 3 * s);
            }
            return r & (BOARD_MASK ^ mask);
        }
    };

    class Solver {
    public:
        std::atomic<bool> abort{ false };   // set from another thread to stop a search
        unsigned long long nodes = 0;

        Solver() : tableSize(nextPrime(1u << 24)), keys(tableSize, 0), values(tableSize, 0) {}

        // Exact score of a position (player to move's perspective).
        int solve(const Position& P) {
            if (P.canWinNext()) return (WIDTH * HEIGHT + 1 - P.movesPlayed()) / 2;
            int min = -(WIDTH * HEIGHT - P.movesPlayed()) / 2;
            int max = (WIDTH * HEIGHT + 1 - P.movesPlayed()) / 2;
            while (min < max) {                // binary search with null windows
                int med = min + (max - min) / 2;
                if (med <= 0 && min / 2 < med) med = min / 2;
                else if (med >= 0 && max / 2 > med) med = max / 2;
                int r = negamax(P, med, med + 1);
                if (r <= med) max = r; else min = r;
            }
            return min;
        }

        // Column (0..6) that is best for the player to move.
        // Prefers: immediate win > any forced win > draw > slowest loss.
        int bestMove(const Position& P) {
            // Opening book: instant answers for the slow early-game positions.
            int booked = bookLookup(P.key());
            if (booked >= 0) return booked;

            for (int col : COLUMN_ORDER)
                if (P.canPlay(col) && P.isWinningMove(col)) return col;

            // Any move that leads to a forced win (one cheap null-window search each).
            for (int col : COLUMN_ORDER) {
                if (!P.canPlay(col)) continue;
                Position P2(P); P2.playCol(col);
                if (P2.canWinNext() || P2.movesPlayed() == WIDTH * HEIGHT) continue;
                if (negamax(P2, -1, 0) < 0) return col;   // opponent loses => we win
            }
            // Otherwise, hold the draw.
            for (int col : COLUMN_ORDER) {
                if (!P.canPlay(col)) continue;
                Position P2(P); P2.playCol(col);
                if (P2.canWinNext()) continue;
                if (P2.movesPlayed() == WIDTH * HEIGHT) return col;
                if (negamax(P2, 0, 1) <= 0) return col;
            }
            // Lost against perfect play: delay as long as possible and hope for a mistake.
            int best = -1, bestScore = -1000;
            for (int col : COLUMN_ORDER) {
                if (!P.canPlay(col)) continue;
                Position P2(P); P2.playCol(col);
                int s = P2.canWinNext() ? -(WIDTH * HEIGHT + 1 - P2.movesPlayed()) / 2 : -solve(P2);
                if (s > bestScore) { bestScore = s; best = col; }
            }
            return best;
        }

    private:
        static int bookLookup(uint64_t key) {
            int lo = 0, hi = OPENING_BOOK_SIZE - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (OPENING_BOOK[mid].key == key) return OPENING_BOOK[mid].col;
                if (OPENING_BOOK[mid].key < key) lo = mid + 1; else hi = mid - 1;
            }
            return -1;
        }

        // Transposition table. Stores the low 32 bits of the 49-bit key; with a
        // prime table size > 2^17 the (index, partial key) pair is still unique.
        // Value encoding: 0 = empty, [1..43] = upper bound, [44..86] = lower bound.
        static constexpr int UPPER_OFFSET = 1 - MIN_SCORE;                  // alpha + 22
        static constexpr int LOWER_OFFSET = MAX_SCORE - 2 * MIN_SCORE + 2;   // score + 65
        static constexpr int LOWER_THRESHOLD = MAX_SCORE - MIN_SCORE + 1;    // 43

        size_t tableSize;
        std::vector<uint32_t> keys;
        std::vector<int8_t> values;

        static size_t nextPrime(size_t n) {
            auto isPrime = [](size_t x) {
                if (x < 2) return false;
                for (size_t d = 2; d * d <= x; d++) if (x % d == 0) return false;
                return true;
                };
            while (!isPrime(n)) n++;
            return n;
        }
        void ttPut(uint64_t key, int8_t v) { size_t i = key % tableSize; keys[i] = (uint32_t)key; values[i] = v; }
        int ttGet(uint64_t key) const { size_t i = key % tableSize; return keys[i] == (uint32_t)key ? values[i] : 0; }

        struct MoveSorter {
            uint64_t moves[WIDTH]; int scores[WIDTH]; int size = 0;
            void add(uint64_t m, int s) {
                int pos = size++;
                for (; pos && scores[pos - 1] > s; --pos) { moves[pos] = moves[pos - 1]; scores[pos] = scores[pos - 1]; }
                moves[pos] = m; scores[pos] = s;
            }
            uint64_t next() { return size ? moves[--size] : 0; }
        };

        // Precondition: the player to move cannot win immediately.
        int negamax(const Position& P, int alpha, int beta) {
            if (abort.load(std::memory_order_relaxed)) return 0;
            nodes++;

            uint64_t next = P.possibleNonLosingMoves();
            if (next == 0) return -(WIDTH * HEIGHT - P.movesPlayed()) / 2;   // every move loses
            if (P.movesPlayed() >= WIDTH * HEIGHT - 2) return 0;               // draw

            int min = -(WIDTH * HEIGHT - 2 - P.movesPlayed()) / 2;
            if (alpha < min) { alpha = min; if (alpha >= beta) return alpha; }
            int max = (WIDTH * HEIGHT - 1 - P.movesPlayed()) / 2;
            if (beta > max) { beta = max; if (alpha >= beta) return beta; }

            uint64_t key = P.key();
            if (int val = ttGet(key)) {
                if (val > LOWER_THRESHOLD) {
                    int lb = val - LOWER_OFFSET;
                    if (alpha < lb) { alpha = lb; if (alpha >= beta) return alpha; }
                }
                else {
                    int ub = val - UPPER_OFFSET;
                    if (beta > ub) { beta = ub; if (alpha >= beta) return beta; }
                }
            }

            MoveSorter sorter;
            for (int i = WIDTH; i--;)
                if (uint64_t move = next & Position::columnMask(COLUMN_ORDER[i]))
                    sorter.add(move, P.moveScore(move));

            while (uint64_t move = sorter.next()) {
                Position P2(P); P2.play(move);
                int score = -negamax(P2, -beta, -alpha);
                if (abort.load(std::memory_order_relaxed)) return 0;
                if (score >= beta) { ttPut(key, (int8_t)(score + LOWER_OFFSET)); return score; }
                if (score > alpha) alpha = score;
            }
            ttPut(key, (int8_t)(alpha + UPPER_OFFSET));
            return alpha;
        }
    };

} // namespace c4