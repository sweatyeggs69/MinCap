// Widescreen support specific to The Legend of Zelda: The Minish Cap (USA, BZME).
//
// Rooms are drawn with two map layers, gMapBottom and gMapTop (MapLayer in the tmc
// decompilation). Each layer's BgSettings pointer (MapLayer + 0) names the background it is
// shown on; its control word gives the screen block. When a room loads, the game renders each
// layer's whole room into a plain tilemap in EWRAM, 128 entries per row (gMapDataBottomSpecial
// and gMapDataTopSpecial, RenderMapLayerToSubTileMap), and every frame copies the part around
// the camera into a 32x23 BG buffer that VBlank DMAs to VRAM. That buffer is aligned to the
// camera's 16-pixel block: buffer column 0 is room pixel (scroll_x - origin_x) & ~15, buffer
// row 1 is room pixel (scroll_y - origin_y) & ~15, and HOFS/VOFS only hold the rest (plus the
// screen shake). For the extra columns of a widescreen view we read tilemap entries straight
// from the room tilemap in RAM:
//
//   gRoomControls (IWRAM 0x03000BF0): +6 origin_x, +8 origin_y, +0xA scroll_x, +0xC scroll_y,
//                                     +0x1E width, +0x20 height (pixels)
//   entry = u16 at special + ((ry >> 3) * 128 + (rx >> 3)) * 2, (rx, ry) = room pixel
//
// Once per frame we check that the entries in RAM match what is actually in VRAM by comparing
// sample tiles of the visible area. A layer that does not match (room transitions, menus,
// cutscenes with other layouts) is hidden at the sides. Gameplay is the frames where at least
// one layer matches; only those widen. Every other background (the HUD and text boxes on BG0,
// clouds and overlays) is hidden at the sides rather than showing stale or wrapped VRAM. Only
// addresses and layout descriptions belong here; never copy game data into the repo.
#include "gba.h"

namespace mincap::gba {

namespace {
constexpr u32 kRoomControls = 0x03000BF0;
constexpr u32 kOriginX = 0x06, kOriginY = 0x08, kScrollX = 0x0A, kScrollY = 0x0C, kWidth = 0x1E, kHeight = 0x20;
struct Layer {
    u32 map_layer;  // MapLayer (+0 BgSettings*)
    u32 tilemap;    // whole-room tilemap, 128 entries per row
};
constexpr Layer kLayers[2] = {
    {0x02025EB0, 0x02019EE0},  // gMapBottom, gMapDataBottomSpecial
    {0x0200B650, 0x02002F00},  // gMapTop, gMapDataTopSpecial
};
constexpr s32 kStride = 128;   // tilemap entries per row
constexpr s32 kMaxTiles = 128; // rooms are at most 64x64 blocks of 16x16 pixels

bool in_iwram(u32 a) { return (a >> 24) == 0x03; }
}  // namespace

u16 Gba::wide_ram16(u32 a) const {
    if ((a >> 24) == 0x02) return u16(ewram_[a & 0x3FFFF] | (ewram_[(a + 1) & 0x3FFFF] << 8));
    if ((a >> 24) == 0x03) return u16(iwram_[a & 0x7FFF] | (iwram_[(a + 1) & 0x7FFF] << 8));
    if ((a >> 24) >= 0x08 && (a >> 24) <= 0x09) {
        const u32 off = a & 0x01FFFFFE;
        if (off + 1 < rom_.size()) return u16(rom_[off] | (rom_[off + 1] << 8));
    }
    return 0;
}

u32 Gba::wide_ram32(u32 a) const { return u32(wide_ram16(a)) | (u32(wide_ram16(a + 2)) << 16); }

u16 Gba::wide_level_entry(int bg, s32 tx, s32 ty) const {
    const WideLevel& L = wide_level_[bg];
    return wide_ram16(L.map + u32(ty * L.stride + tx) * 2);
}

void Gba::wide_level_detect() {
    for (auto& L : wide_level_) L = WideLevel{};
    wide_scene_ = false;
    wide_main_bg_ = -1;
    wide_hud_prio_ = -1;
    if (!wide_margin_) return;
    const u16 dispcnt = u16(io_[0] | (io_[1] << 8));
    if ((dispcnt & 7) != 0) return;

    const s32 width = s32(wide_ram16(kRoomControls + kWidth)), height = s32(wide_ram16(kRoomControls + kHeight));
    if (width <= 0 || height <= 0 || width > kMaxTiles * 8 || height > kMaxTiles * 8) return;
    const s32 dx = s32(s16(wide_ram16(kRoomControls + kScrollX) - wide_ram16(kRoomControls + kOriginX)));
    const s32 dy = s32(s16(wide_ram16(kRoomControls + kScrollY) - wide_ram16(kRoomControls + kOriginY)));
    const s32 tiles_w = (width + 7) / 8, tiles_h = (height + 7) / 8;

    int first_bg = -1;
    for (int layer = 0; layer < 2; ++layer) {
        const u32 settings = wide_ram32(kLayers[layer].map_layer);
        if (!in_iwram(settings)) continue;
        const u16 want = wide_ram16(settings);  // BgSettings.control
        int bg = -1;
        for (int b = 0; b < 4; ++b) {
            const u16 cnt = u16(io_[0x08 + 2 * b] | (io_[0x09 + 2 * b] << 8));
            if (!(dispcnt & (0x100 << b)) || ((cnt >> 8) & 31) != ((want >> 8) & 31)) continue;
            if ((cnt & 0x80) || ((cnt >> 14) & 3) != 0) continue;  // 16-color, 256x256 only
            bg = b;
            break;
        }
        if (bg < 0 || wide_level_[bg].mode == WideMode::Level) continue;

        const u16 cnt = u16(io_[0x08 + 2 * bg] | (io_[0x09 + 2 * bg] << 8));
        const u32 hofs = u32(io_[0x10 + 4 * bg] | (io_[0x11 + 4 * bg] << 8)) & 0x1FF;
        const u32 vofs = u32(io_[0x12 + 4 * bg] | (io_[0x13 + 4 * bg] << 8)) & 0x1FF;
        // Room pixel at screen (0, 0) minus the register offsets (which may be "negative"
        // during a screen shake, as 9-bit values).
        WideLevel& L = wide_level_[bg];
        L.layer = layer;
        L.map = kLayers[layer].tilemap;
        L.stride = kStride;
        L.width = width;
        L.height = height;
        L.tiles_w = tiles_w;
        L.tiles_h = tiles_h;
        // The game runs the next frame's logic during VBlank, so by the time the frame is drawn
        // gRoomControls may already hold the next camera while VRAM still shows this one. Try
        // the camera from RAM first, then the neighbouring 16-pixel blocks, and keep the one
        // VRAM agrees with.
        const s32 base_x = (dx & ~15) - (hofs >= 256 ? 512 : 0);
        const s32 base_y = (dy & ~15) - 8 - (vofs >= 256 ? 512 : 0);

        // Sample tiles spread over the visible area; at least 15 of every 16 must agree
        // (animated tiles and doors can differ for a frame).
        const u32 screen_base = ((cnt >> 8) & 31) * 0x800;
        static constexpr int kSx[] = {4, 36, 68, 100, 132, 164, 196, 228};
        static constexpr int kSy[] = {12, 60, 108, 148};
        auto score = [&](s32 cam_x, s32 cam_y) {
            int checked = 0, match = 0;
            for (int sy : kSy)
                for (int sx : kSx) {
                    const s32 ax = sx + s32(hofs) + cam_x, ay = sy + s32(vofs) + cam_y;
                    if (ax < 0 || ay < 0 || ax >= tiles_w * 8 || ay >= tiles_h * 8) continue;
                    const u32 vx = (u32(sx) + hofs) & 255, vy = (u32(sy) + vofs) & 255;
                    const u32 off = screen_base + (vy >> 3) * 64 + (vx >> 3) * 2;
                    const u16 vram_e = u16(vram_[off] | (vram_[off + 1] << 8));
                    ++checked;
                    if (vram_e == wide_level_entry(bg, ax >> 3, ay >> 3)) ++match;
                }
            return checked >= 8 && match * 16 >= checked * 15 ? match : -1;
        };
        // RAM's camera first (it wins ties), then the nearest blocks outwards.
        static constexpr int kSteps[] = {0, -1, 1, -2, 2};
        int best = -1;
        for (int ky : kSteps)
            for (int kx : kSteps) {
                const int m = score(base_x + kx * 16, base_y + ky * 16);
                if (m > best) {
                    best = m;
                    L.cam_x = base_x + kx * 16;
                    L.cam_y = base_y + ky * 16;
                }
            }
        if (best < 0) {
            L = WideLevel{};
            continue;
        }
        L.mode = WideMode::Level;
        if (first_bg < 0 || layer == 0) first_bg = bg;
    }

    if (first_bg < 0) return;
    wide_scene_ = true;
    // Side columns past the room's edge are black; the bottom layer decides.
    wide_main_bg_ = first_bg;
    // The HUD's buttons and counters are priority 0 objects; when the HUD slides away the game
    // parks them just past the right edge, so they stay inside the original view.
    wide_hud_prio_ = 0;
    // Everything that is not a matched room layer (the HUD and text boxes on BG0, overlays)
    // would show stale or wrapped VRAM: hide its sides.
    for (int bg = 0; bg < 4; ++bg)
        if (wide_level_[bg].mode != WideMode::Level) wide_level_[bg].mode = WideMode::Hide;
}

}  // namespace mincap::gba
