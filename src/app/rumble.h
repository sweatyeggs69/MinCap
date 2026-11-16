#pragma once
// Controller rumble (Settings > Controls > Rumble). The GBA has no rumble, so Watcher reads the
// game's RAM after every frame and turns what just happened into a Pulse; the host plays it on
// every connected controller (input::Manager::rumble), scaled by the strength setting.
//
// Events, read from The Minish Cap's RAM (addresses from the community decompilation; the stats
// are the ones the cheats use, gRoomControls the one the widescreen view uses):
//   - Link takes damage: gSave.stats (EWRAM 0x02002AE8) +2 u8 health drops with the maximum (+3)
//     unchanged while the game task runs (gMain, IWRAM 0x03001002, +0 == 2). Stronger for
//     bigger hits.
//   - Link falls: health reaches 0.
//   - the screen shakes: gRoomControls (IWRAM 0x03000BF0) +0x16 u16 shake time goes up (+0x14 u8
//     strength, 0-7). The game shakes the screen for bosses, explosions, falling rocks and the
//     like, so the controller shakes with it.
//
// Settings (mincap::Config): input.rumble (on by default), input.rumble_strength (10-100,
// default 75).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "game.h"

namespace mincap::rumble {

inline constexpr bool kDefaultOn = true;
inline constexpr int kDefaultStrength = 75;
inline constexpr int kMinStrength = 10;
// Shown next to the Controller rumble toggle.
inline constexpr const char* kEventsHelp =
    "Shakes your controller when Link is hurt, harder for bigger hits, when he runs out of "
    "hearts and when the screen shakes (bosses, explosions, falling rocks). Works with "
    "controllers that have rumble motors.";

// One rumble: motor strengths 0..1 (low = the big, slow motor, high = the small, fast one).
struct Pulse {
    float low = 0, high = 0;
    int ms = 0;
    explicit operator bool() const { return ms > 0 && (low > 0 || high > 0); }
    float strength() const { return std::max(low, high); }
};

// The stronger of two pulses; on a tie the longer one.
inline Pulse stronger(const Pulse& a, const Pulse& b) {
    if (!a) return b;
    if (!b) return a;
    if (a.strength() != b.strength()) return a.strength() > b.strength() ? a : b;
    return a.ms >= b.ms ? a : b;
}

// Decides which pulses reach the controllers: a new pulse replaces the one still playing only
// when it is at least as strong, so a light tick never cuts a big hit short. Counts time in
// game frames (step once per emulated frame).
class Mixer {
public:
    // True when p should be played now.
    bool take(const Pulse& p) {
        if (!p) return false;
        if (left_ms_ > 0 && p.strength() < playing_.strength()) return false;
        playing_ = p;
        left_ms_ = p.ms;
        return true;
    }
    void step() { left_ms_ = std::max(0, left_ms_ - kFrameMs); }
    bool playing() const { return left_ms_ > 0; }
    void clear() { left_ms_ = 0; }

private:
    static constexpr int kFrameMs = 17;  // one GBA frame (16.74 ms)
    Pulse playing_;
    int left_ms_ = 0;
};

// Strength setting (percent) -> motor value for SDL (0..0xFFFF).
inline std::uint16_t motor(float level, int strength_percent) {
    const float v = std::clamp(level, 0.0f, 1.0f) * std::clamp(strength_percent, 0, 100) / 100.0f;
    return std::uint16_t(v * 65535.0f + 0.5f);
}

// A hit that took `lost` of `max` health: stronger and longer for bigger hits.
inline Pulse hit(int lost, int max) {
    const float share = max > 0 ? float(lost) / float(max) : 0.0f;
    const float f = std::min(1.0f, 0.45f + share * 2.0f);
    return {f, f * 0.6f, 120 + int(std::min(share, 0.5f) * 400.0f)};
}
inline constexpr Pulse kDeath{1.0f, 1.0f, 700};

namespace detail {
inline std::uint16_t get16(std::span<const std::uint8_t> m, std::size_t at) {
    return std::uint16_t(m[at] | (m[at + 1] << 8));
}
}  // namespace detail

// ---- The Legend of Zelda: The Minish Cap ----

inline constexpr unsigned kRoomControls = 0x03000BF0;

class Watcher {
public:
    // Forgets the last frame, so a jump in the numbers (a loaded state, rewind, a new game) is
    // not taken for an event.
    void reset() { primed_ = false; }

    // Call after each emulated frame with the 32 KiB IWRAM (0x03000000) and 256 KiB EWRAM.
    Pulse step(std::span<const std::uint8_t> iwram, std::span<const std::uint8_t> ewram) {
        using detail::get16;
        constexpr std::size_t iw = 0x03000000u, ew = 0x02000000u;
        const std::size_t stats = game::kStatsAddress - ew;
        if (iwram.size() < 0x8000 || ewram.size() < stats + 4) return {};
        Frame now;
        now.in_game = iwram[game::kMainTaskAddress - iw] == game::kTaskGame;
        now.health = ewram[stats + 2];
        now.max_health = ewram[stats + 3];
        now.shake = get16(iwram, kRoomControls - iw + 0x16);
        now.shake_power = iwram[kRoomControls - iw + 0x14] & 7;

        const Frame before = last_;
        const bool primed = primed_;
        last_ = now;
        primed_ = true;
        if (!primed || !now.in_game || !before.in_game) return {};

        Pulse out;
        // A loaded file: 3 to 20 hearts of 8 units each, same maximum as last frame.
        const bool file = now.max_health >= 3 * 8 && now.max_health <= 20 * 8 && now.max_health % 8 == 0;
        if (file && now.max_health == before.max_health && now.health < before.health)
            out = now.health == 0 ? kDeath : hit(before.health - now.health, now.max_health);
        if (now.shake > before.shake) {
            const float f = std::min(1.0f, 0.45f + now.shake_power * 0.08f);
            out = stronger(out, {f, f * 0.5f, std::clamp(int(now.shake) * 17, 120, 900)});
        }
        return out;
    }

private:
    struct Frame {
        std::uint8_t health = 0, max_health = 0, shake_power = 0;
        std::uint16_t shake = 0;
        bool in_game = false;
    };
    Frame last_;
    bool primed_ = false;
};

}  // namespace mincap::rumble
