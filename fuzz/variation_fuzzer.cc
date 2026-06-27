#include <cstdint>
#include <cstddef>
#include <algorithm>
#include "fontscope/inspect.h"
#include "fontscope/glyph_loader.h"
#include "fontscope/variation.h"
#include "fontscope/sfnt.h"

using namespace fontscope;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;

    const FontFace& font = font_res.value;
    uint16_t n = font.maxp.num_glyphs;
    if (n > 16) n = 16;

    LoadOptions opts;
    opts.ppem            = 16;
    opts.apply_hints     = false;
    opts.apply_variation = true;
    opts.apply_iup       = false;

    VariationStore vstore;
    const TableRecord* gvar_rec = find_table(font.sfnt, tags::GVAR());
    if (gvar_rec) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(gvar_rec->offset, gvar_rec->length);
        uint16_t n_axes = font.has_fvar ? uint16_t(font.fvar.axes.size()) : 0;
        auto gv = parse_gvar(sub, font.maxp.num_glyphs, n_axes);
        if (gv.ok()) vstore = std::move(gv.value);
    }

    if (font.has_fvar && !font.fvar.axes.empty()) {
        opts.normalized_coords.resize(font.fvar.axes.size());
        for (size_t i = 0; i < font.fvar.axes.size(); ++i)
            opts.normalized_coords[i] = F2Dot14::from_f64(0.5);
    }

    for (uint16_t gid = 0; gid < n; ++gid) {
        HintContext ctx = make_hint_context(font, gid, opts.ppem);
        (void)load_and_process_glyph(font, ctx, vstore, gid, opts);
    }

    return 0;
}
