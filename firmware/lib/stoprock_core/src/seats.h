// Who sits where: the one place that says which player's times go in which column, for every mode.
//
// Players are 0 lead, 1 second, 2 third, 3 skip, kThem (the other team), or kNoPlayer. A "seat" is
// where the person holding the stopwatch sits: their own position (0-3: competition and league,
// where they time their three teammates), or kCoachSeat (a coach, who times all four). The columns
// are the teammates in throwing order, then THEM: four for a player, five for a coach.
#pragma once
#include <stdint.h>

namespace stoprock {

constexpr uint8_t kThem = 4;          // a stone of the other team's
constexpr uint8_t kNoPlayer = 0xFF;   // not known (or no players, in games before format 4)
constexpr uint8_t kCoachSeat = 4;     // a coach: every position has a column
constexpr uint8_t kLeagueColumns = 4, kCoachColumns = 5;  // a player's columns, a coach's

inline uint8_t columnCount(uint8_t seat) { return seat > 3 ? kCoachColumns : kLeagueColumns; }
inline uint8_t themColumn(uint8_t seat) { return columnCount(seat) - 1; }

// The player whose times are in column col.
inline uint8_t columnPlayer(uint8_t seat, uint8_t col) {
    if (col >= themColumn(seat)) return kThem;
    return seat > 3 || col < seat ? col : col + 1;
}

// The column of a stone thrown by player; -1 for none (your own, or no player).
inline int playerColumn(uint8_t seat, uint8_t player) {
    if (player == kThem) return themColumn(seat);
    if (player > 3 || player == seat) return -1;
    return player < seat ? player : player - 1;
}

}  // namespace stoprock
