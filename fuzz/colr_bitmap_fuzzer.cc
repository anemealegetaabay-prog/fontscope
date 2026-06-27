#include <cstdint>
#include <cstddef>
#include <algorithm>
#include "fontscope/inspect.h"
#include "fontscope/pipeline.h"
#include "fontscope/colr.h"
#include "fontscope/cpal.h"

using namespace fontscope;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;

    const FontFace& font = font_res.value;
    if (!font.has_colr || !font.has_cpal) return 0;

    PipelineConfig pcfg;
    pcfg.ppem        = 16;
    pcfg.antialiased = false;
    pcfg.apply_hints = false;
    pcfg.palette_id  = 0;

    uint16_t clim = std::min<uint16_t>(uint16_t(font.colr.glyphs.size()), 16);
    for (uint16_t i = 0; i < clim; ++i) {
        uint16_t cgid = font.colr.glyphs[i].glyph_id;
        (void)render_color_glyph(font, cgid, pcfg);
    }

    return 0;
}
