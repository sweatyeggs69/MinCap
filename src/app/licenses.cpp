#include "app/licenses.h"

#include <cstdio>

extern const unsigned char mincap_text_license[];
extern const unsigned mincap_text_license_size;
extern const unsigned char mincap_text_notices[];
extern const unsigned mincap_text_notices_size;
extern const unsigned char mincap_text_dmsans_ofl[];
extern const unsigned mincap_text_dmsans_ofl_size;

namespace mincap::licenses {

namespace {
std::string_view view(const unsigned char* p, unsigned n) { return {reinterpret_cast<const char*>(p), n}; }
}  // namespace

std::string_view mincap() { return view(mincap_text_license, mincap_text_license_size); }
std::string_view notices() { return view(mincap_text_notices, mincap_text_notices_size); }
std::string_view font_ofl() { return view(mincap_text_dmsans_ofl, mincap_text_dmsans_ofl_size); }

void print_all() {
    for (std::string_view t : {mincap(), notices(), font_ofl()}) {
        std::fwrite(t.data(), 1, t.size(), stdout);
        std::fputs("\n\n", stdout);
    }
}

}  // namespace mincap::licenses
