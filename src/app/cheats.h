#pragma once
// Cheats menu toggles (F1 > Cheats). After every game frame the host calls apply() on the game's
// EWRAM, which tops the chosen stats back up: hearts to the current maximum, rupees to what the
// wallet holds, bombs and arrows to what the bomb bag and quiver hold. Nothing is written unless
// the game task is running and the stats look like a loaded file, so the title and file select
// screens are left alone. Off while RetroAchievements hardcore mode is on. The cheats never give
// an item: infinite bombs only matters once Link has the bomb bag, and the same for arrows.
//
// Settings (mincap::Config): cheats.hearts, cheats.rupees, cheats.bombs, cheats.arrows (all off
// by default).
#include <cstddef>
#include <cstdint>
#include <span>

#include "game.h"

namespace mincap::cheats {

struct Flags {
    bool hearts = false;
    bool rupees = false;
    bool bombs = false;
    bool arrows = false;
    bool any() const { return hearts || rupees || bombs || arrows; }
};

namespace detail {
inline constexpr std::uint32_t kEwram = 0x02000000;
inline constexpr std::uint8_t kMaxHealth = 20 * 8;  // 20 hearts, 8 units each
// Offsets into gSave.stats.
inline constexpr std::size_t kWallet = 0x00, kHealth = 0x02, kMaxHp = 0x03, kBombs = 0x04, kArrows = 0x05,
                             kBombBag = 0x06, kQuiver = 0x07, kRupees = 0x18;
}  // namespace detail

// Applies the enabled cheats to ewram (the 256 KiB EWRAM, starting at 0x02000000). task is the
// game's current task (game::kMainTaskAddress). Returns true when anything changed.
inline bool apply(std::span<std::uint8_t> ewram, std::uint8_t task, const Flags& f) {
    using namespace detail;
    const std::size_t s = game::kStatsAddress - kEwram;
    if (!f.any() || task != game::kTaskGame || ewram.size() < s + kRupees + 2) return false;
    const std::uint8_t max_hp = ewram[s + kMaxHp];
    // A loaded file has at least three hearts and every container size in range.
    if (max_hp < 3 * 8 || max_hp > kMaxHealth || (max_hp % 8) != 0) return false;
    if (ewram[s + kWallet] > 3 || ewram[s + kBombBag] > 3 || ewram[s + kQuiver] > 3) return false;
    bool changed = false;
    auto set8 = [&](std::size_t at, unsigned v) {
        if (ewram[s + at] != std::uint8_t(v)) {
            ewram[s + at] = std::uint8_t(v);
            changed = true;
        }
    };
    if (f.hearts) set8(kHealth, max_hp);
    if (f.bombs) set8(kBombs, game::kBombBagSizes[ewram[s + kBombBag]]);
    if (f.arrows) set8(kArrows, game::kQuiverSizes[ewram[s + kQuiver]]);
    if (f.rupees) {
        const unsigned full = game::kWalletSizes[ewram[s + kWallet]];
        if ((ewram[s + kRupees] | (ewram[s + kRupees + 1] << 8)) != int(full)) {
            ewram[s + kRupees] = std::uint8_t(full);
            ewram[s + kRupees + 1] = std::uint8_t(full >> 8);
            changed = true;
        }
    }
    return changed;
}

}  // namespace mincap::cheats
