#pragma once
// The one place that describes the game MinCap runs. Everything game-specific that the rest of
// the code needs (names shown to the user, ROM fingerprint, save layout) lives here, so the
// launcher, packaging and tests do not hard-code it.
//
// Target: The Legend of Zelda: The Minish Cap, USA release (AGB-BZME), revision 0.
//
// Nothing here is game data: these are only identifiers and a fingerprint of the user's own ROM.
// RAM addresses come from the zeldaret/tmc decompilation (linker.ld), which builds this exact ROM.

#include <cstddef>
#include <string_view>

namespace mincap::game {

// Shown in the launcher, dialogs and error messages.
inline constexpr const char* kTitle = "The Legend of Zelda: The Minish Cap";
inline constexpr const char* kRegion = "USA";
inline constexpr const char* kTitleWithRegion = "The Legend of Zelda: The Minish Cap (USA)";
// Used for shortcuts, default export names, etc. (no characters that are invalid in file names).
inline constexpr const char* kFileSafeTitle = "The Legend of Zelda - The Minish Cap";

// GBA cartridge header of the supported dump.
inline constexpr const char* kHeaderTitle = "GBAZELDA MC";  // 0xA0, 12 bytes
inline constexpr const char* kGameCode = "BZME";            // 0xAC, 4 bytes
inline constexpr unsigned kRevision = 0;                    // 0xBC
inline constexpr std::size_t kRomSize = 16 * 1024 * 1024;

// SHA-1 of the known-good dump (lowercase hex), checked against the maintainer's own cartridge
// dump on 2026-11-13 (MD5 a104896da0047abe8bee2a6e3f4c7290, CRC32 ABCEBBB1). It is also the SHA-1 the tmc
// decompilation builds. If this is ever emptied, a ROM is accepted on a header match instead and
// the launcher labels it "header match (fingerprint not pinned yet)".
inline constexpr const char* kExpectedSha1 = "b4bd50e4131b027c334547b4524e2dbbd4227130";

// Battery save: 64 Kbit (8 KiB) serial EEPROM (the ROM carries the EEPROM_V124 library string).
// The runtime picks the chip from that string; this is the size of the save file it writes.
inline constexpr std::size_t kSaveSize = 8 * 1024;
inline constexpr const char* kSaveChip = "EEPROM";
// Bytes the game writes at the start of its save, used to reject saves from other games. Empty
// disables the check. The game writes its signature "AGBZELDA:THE MINISH CAP:ZELDA 5" (32 bytes
// with the closing zero) at EEPROM address 0 on first boot (InitSaveData in the decompilation).
// The save file holds each 64-bit EEPROM block byte-reversed ("AGBZELDA" is stored "ADLEZBGA"),
// so that is the form checked here. Confirmed on 2026-11-13 against the save the game creates on
// first boot. It holds a zero byte, so the length is explicit.
inline constexpr std::string_view kSaveHeader{"ADLEZBGA" "NIM EHT:" ":PAC HSI" "\x00" "5 ADLEZ", 32};

// Widescreen view (16:10 and 16:9). The extra columns are rebuilt from the room in RAM by
// src/gba/wide_level.cpp; false hides the option and keeps the picture 3:2.
inline constexpr bool kWidescreen = true;

// gMain (IWRAM): +2 u8 task. Cheats only touch the save while the game task runs (not on the
// title or file select screens, where gSave may hold another file or nothing).
inline constexpr unsigned kMainTaskAddress = 0x03001002;
inline constexpr unsigned kTaskGame = 2;

// gSave.stats (EWRAM 0x02002A40 + 0xA8), used by the cheats menu:
//   +0 u8 wallet type (0-3)   +2 u8 health   +3 u8 max health (8 per heart)
//   +4 u8 bombs   +5 u8 arrows   +6 u8 bomb bag type (0-3)   +7 u8 quiver type (0-3)
//   +0x18 u16 rupees
inline constexpr unsigned kStatsAddress = 0x02002AE8;
inline constexpr unsigned kWalletSizes[4] = {100, 300, 500, 999};
inline constexpr unsigned kBombBagSizes[4] = {10, 30, 50, 99};
inline constexpr unsigned kQuiverSizes[4] = {30, 50, 70, 99};

}  // namespace mincap::game
