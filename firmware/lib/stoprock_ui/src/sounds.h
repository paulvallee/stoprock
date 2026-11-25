// The sound settings (the Sounds page in the mode menu): everything muted, or a level for each kind of
// sound, 0 (off) to 4. Level 3 is each sound as designed; the others scale its own volume.
#pragma once
#include <stdint.h>

namespace stoprock {

enum class SoundKind : uint8_t { Click, Made, Missed, Undo, Error };
constexpr int kSoundKinds = 5;
constexpr uint8_t kSoundLevels = 4;        // the meter's bars; 0 is off
constexpr uint8_t kDefaultSoundLevel = 3;  // as designed

struct SoundSettings {
    bool muted = false;
    uint8_t level[kSoundKinds] = {kDefaultSoundLevel, kDefaultSoundLevel, kDefaultSoundLevel,
                                  kDefaultSoundLevel, kDefaultSoundLevel};

    uint8_t of(SoundKind k) const { return level[static_cast<uint8_t>(k)]; }
    // The next level, a press on its row: up a bar, and from the top round to off.
    void step(SoundKind k) {
        uint8_t& l = level[static_cast<uint8_t>(k)];
        l = l >= kSoundLevels ? 0 : l + 1;
    }
    // Settings read back from storage: anything out of range goes back to the default.
    void sanitize() {
        muted = muted != 0;
        for (uint8_t& l : level)
            if (l > kSoundLevels) l = kDefaultSoundLevel;
    }
};

// The buzzer drive for a note of a sound (a percentage of the original buzzer's, as board::tone takes),
// at its kind's level: 0 when that sound is off, or when muted (unless previewing a level on the page).
inline uint8_t soundVolume(const SoundSettings& s, SoundKind k, uint8_t noteVolume, bool preview = false) {
    static constexpr uint8_t kScale[kSoundLevels + 1] = {0, 15, 40, 100, 150};  // percent of the note's
    const uint8_t level = s.of(k);
    if ((s.muted && !preview) || level == 0 || level > kSoundLevels || noteVolume == 0) return 0;
    const unsigned v = (noteVolume * kScale[level] + 50) / 100;
    return v < 1 ? 1 : v > 255 ? 255 : static_cast<uint8_t>(v);
}

}  // namespace stoprock
