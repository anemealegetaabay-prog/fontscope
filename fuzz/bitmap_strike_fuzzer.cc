#include <cstdint>
#include <cstddef>
#include "fontscope/inspect.h"
#include "fontscope/sbix.h"

using namespace fontscope;

// Exercises the sbix embedded-bitmap path: strike + per-glyph offset parsing
// and image-header decoding for each stored glyph bitmap.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;
    const FontFace& font = font_res.value;
    if (!font.has_sbix) return 0;

    for (const auto& strike : font.sbix.strikes) {
        for (const auto& g : strike.glyphs) {
            if (g.empty()) continue;
            SbixImageSize sz = sbix_image_size(g);
            (void)sz;
        }
    }

    return 0;
}
