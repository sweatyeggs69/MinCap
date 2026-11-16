// Rumble: which changes in blank RAM images turn into controller pulses. No ROM needed.
#include <array>
#include <cstdint>
#include <cstdio>

#include "app/rumble.h"

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
using Iwram = std::array<std::uint8_t, 0x8000>;
using Ewram = std::array<std::uint8_t, 0x40000>;
template <typename M> void set16(M& m, std::size_t at, std::uint16_t v) { m[at] = std::uint8_t(v); m[at + 1] = std::uint8_t(v >> 8); }
constexpr std::size_t ew(unsigned a) { return a - 0x02000000u; }
constexpr std::size_t iw(unsigned a) { return a - 0x03000000u; }
}  // namespace

int main() {
    namespace rumble = mincap::rumble;
    namespace game = mincap::game;
    Iwram iwram{};
    Ewram ram{};
    rumble::Watcher w;
    const std::size_t stats = ew(game::kStatsAddress);
    const std::size_t shake = iw(rumble::kRoomControls) + 0x16;

    // File select (task 1): nothing, even when the numbers change.
    iwram[iw(game::kMainTaskAddress)] = 1;
    ram[stats + 3] = 3 * 8;
    ram[stats + 2] = 3 * 8;
    w.step(iwram, ram);
    ram[stats + 2] = 8;
    CHECK(!w.step(iwram, ram));

    // In game with 3 of 3 hearts.
    iwram[iw(game::kMainTaskAddress)] = game::kTaskGame;
    ram[stats + 2] = 3 * 8;
    w.step(iwram, ram);
    CHECK(!w.step(iwram, ram));

    // Half a heart.
    ram[stats + 2] = 3 * 8 - 4;
    rumble::Pulse p = w.step(iwram, ram);
    CHECK(p && p.low >= 0.45f && p.strength() < 1.0f);

    // Out of hearts.
    ram[stats + 2] = 0;
    p = w.step(iwram, ram);
    CHECK(p && p.strength() == 1.0f);

    // A fairy brings him back: nothing.
    ram[stats + 2] = 3 * 8;
    CHECK(!w.step(iwram, ram));

    // A heart container raises the maximum: nothing.
    ram[stats + 3] = 4 * 8;
    CHECK(!w.step(iwram, ram));

    // The screen shakes for 40 frames at strength 3, then counts down.
    set16(iwram, shake, 40);
    iwram[iw(rumble::kRoomControls) + 0x14] = 3;
    p = w.step(iwram, ram);
    CHECK(p && p.low >= 0.5f && p.ms >= 40 * 16);
    set16(iwram, shake, 39);
    CHECK(!w.step(iwram, ram));

    // reset(): a loaded state with less health is not a hit.
    w.reset();
    ram[stats + 2] = 8;
    CHECK(!w.step(iwram, ram));

    // Mixer: a weaker pulse never cuts a stronger one short; it plays again once that is over.
    rumble::Mixer mix;
    CHECK(mix.take({1.0f, 1.0f, 100}));
    mix.step();
    CHECK(!mix.take({0.0f, 0.35f, 70}));
    for (int i = 0; i < 10; ++i) mix.step();
    CHECK(!mix.playing());
    CHECK(mix.take({0.0f, 0.35f, 70}));
    CHECK(!mix.take({}));

    // Strength scaling.
    CHECK(rumble::motor(1.0f, 100) == 0xFFFF);
    CHECK(rumble::motor(1.0f, 50) == 0x8000);
    CHECK(rumble::motor(0.0f, 100) == 0);
    CHECK(rumble::motor(2.0f, 100) == 0xFFFF);

    if (failures) return 1;
    std::puts("rumble: ok");
    return 0;
}
