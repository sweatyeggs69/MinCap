// MinCap's colors for the shared launcher theme (theme.h): deep forest-green surfaces, a Minish
// Cap green accent (Link's cap and tunic) and a gold focus ring, after the gold rim of the
// MinCap logo. This is the only theme file that differs between ports.
#include "app/theme.h"

namespace mincap::theme {

namespace {
ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
}  // namespace

const Palette& palette() {
    static const Palette p{
        rgb(11, 22, 14),            // surface
        rgb(17, 31, 20),            // card
        rgb(24, 42, 28),            // raised
        rgb(32, 55, 37),            // high
        rgb(170, 230, 170, 0.15f),  // line
        rgb(242, 250, 240),         // ink
        rgb(184, 210, 182),         // ink_muted
        rgb(124, 152, 124),         // ink_faint
        rgb(72, 176, 64),           // accent (Minish Cap green)
        rgb(104, 200, 92),          // accent_hover
        rgb(52, 146, 48),           // accent_active
        rgb(72, 176, 64, 0.16f),    // accent_soft
        rgb(8, 24, 8),              // on_accent
        rgb(255, 214, 72),          // highlight (gold)
        rgb(52, 211, 153),          // good
        rgb(248, 113, 113),         // bad
        rgb(30, 110, 40),           // glow
    };
    return p;
}

}  // namespace mincap::theme
