// Cheats: refills Link's hearts, rupees, bombs and arrows in a blank EWRAM image. No ROM needed.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "app/cheats.h"

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
constexpr std::size_t kStats = 0x2AE8;
constexpr std::uint8_t kGame = 2, kFileSelect = 1;
std::uint16_t get16(const std::vector<std::uint8_t>& m, std::size_t at) { return std::uint16_t(m[at] | (m[at + 1] << 8)); }
void set16(std::vector<std::uint8_t>& m, std::size_t at, std::uint16_t v) { m[at] = std::uint8_t(v); m[at + 1] = std::uint8_t(v >> 8); }
}  // namespace

int main() {
    namespace cheats = mincap::cheats;
    using cheats::Flags;
    std::vector<std::uint8_t> ram(0x40000, 0);
    const Flags all{true, true, true, true};

    // Nothing enabled, or no file loaded (max health 0): no writes.
    CHECK(!cheats::apply(ram, kGame, Flags{}));
    CHECK(!cheats::apply(ram, kGame, all));
    CHECK(ram[kStats + 2] == 0 && get16(ram, kStats + 0x18) == 0);

    // A new file: 3 hearts (24), 2 hearts left, small wallet with 40 rupees, no bags.
    ram[kStats + 3] = 24;
    ram[kStats + 2] = 16;
    set16(ram, kStats + 0x18, 40);
    // Not in the game task (title, file select): left alone.
    CHECK(!cheats::apply(ram, kFileSelect, all));
    CHECK(ram[kStats + 2] == 16);

    CHECK(cheats::apply(ram, kGame, Flags{true, false, false, false}));
    CHECK(ram[kStats + 2] == 24 && ram[kStats + 3] == 24);  // max never changes
    CHECK(get16(ram, kStats + 0x18) == 40);
    CHECK(!cheats::apply(ram, kGame, Flags{true, false, false, false}));  // already full

    CHECK(cheats::apply(ram, kGame, Flags{false, true, false, false}));
    CHECK(get16(ram, kStats + 0x18) == 100);
    ram[kStats + 0] = 3;  // biggest wallet
    CHECK(cheats::apply(ram, kGame, Flags{false, true, false, false}));
    CHECK(get16(ram, kStats + 0x18) == 999);

    ram[kStats + 6] = 1;  // big bomb bag (30)
    ram[kStats + 7] = 2;  // quiver of 70
    CHECK(cheats::apply(ram, kGame, Flags{false, false, true, true}));
    CHECK(ram[kStats + 4] == 30 && ram[kStats + 5] == 70);

    // Garbage (not a loaded file): no writes.
    ram[kStats + 3] = 25;
    ram[kStats + 2] = 1;
    CHECK(!cheats::apply(ram, kGame, all));
    ram[kStats + 3] = 24;
    ram[kStats + 6] = 9;
    CHECK(!cheats::apply(ram, kGame, all));
    CHECK(ram[kStats + 2] == 1);

    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    else std::printf("cheats tests passed\n");
    return failures ? 1 : 0;
}
