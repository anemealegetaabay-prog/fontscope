#include <cstdint>
#include <cstddef>
#include "fontscope/inspect.h"
#include "fontscope/validate.h"
#include "fontscope/glyph_loader.h"
#include "fontscope/pipeline.h"
#include "fontscope/hint_vm.h"
#include "fontscope/variation.h"
#include "fontscope/colr.h"
#include "fontscope/cpal.h"
#include "fontscope/bitmap.h"

using namespace fontscope;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    // Load and parse the font — this exercises SFNT, head, maxp, hhea, hmtx,
    // name, cmap, loca, glyf, OS/2, post, COLR, CPAL, fvar, avar.
    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;

    const FontFace& font = font_res.value;

    // Validate — exercises all cross-table consistency checks.
    (void)validate_font(font);

    // Iterate glyphs and exercise the glyph loading path.
    uint16_t n = font.maxp.num_glyphs;
    if (n > 64) n = 64;  // limit time per input

    LoadOptions opts;
    opts.ppem            = 16;
    opts.apply_hints     = true;
    opts.apply_variation = true;
    opts.apply_iup       = true;

    VariationStore vstore;
    const TableRecord* gvar_rec = find_table(font.sfnt, tags::GVAR());
    if (gvar_rec) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(gvar_rec->offset, gvar_rec->length);
        uint16_t n_axes = font.has_fvar ? uint16_t(font.fvar.axes.size()) : 0;
        auto gv = parse_gvar(sub, font.maxp.num_glyphs, n_axes);
        if (gv.ok()) vstore = std::move(gv.value);
    }

    // If the font has variation axes, set a non-default coordinate.
    if (font.has_fvar && !font.fvar.axes.empty()) {
        opts.normalized_coords.resize(font.fvar.axes.size());
        for (size_t i = 0; i < font.fvar.axes.size(); ++i)
            opts.normalized_coords[i] = F2Dot14::from_f64(0.5);
    }

    for (uint16_t gid = 0; gid < n; ++gid) {
        HintContext ctx = make_hint_context(font, gid, opts.ppem);

        // Full load+hint+IUP+variation path.
        auto pg = load_and_process_glyph(font, ctx, vstore, gid, opts);
        if (!pg.ok()) continue;

        // Rasterize if we got a valid outline.
        if (!pg.value.is_empty && !pg.value.points.empty()) {
            RasterBuf buf;
            render_glyph(pg.value.points, pg.value.end_pts,
                         font.head.units_per_em, 16, buf);
        }
    }

    // Exercise COLR color layer blending path.
    if (font.has_colr && font.has_cpal) {
        PipelineConfig pcfg;
        pcfg.ppem         = 16;
        pcfg.antialiased  = false;
        pcfg.apply_hints  = false;
        pcfg.palette_id   = 0;

        uint16_t clim = std::min<uint16_t>(uint16_t(font.colr.glyphs.size()), 4);
        for (uint16_t i = 0; i < clim; ++i) {
            uint16_t cgid = font.colr.glyphs[i].glyph_id;
            (void)render_color_glyph(font, cgid, pcfg);
        }
    }

    // Exercise cmap lookup for a sample of Unicode values.
    for (uint32_t cp = 0x20; cp < 0x100; cp += 16)
        (void)font.cmap.lookup(cp);

    return 0;
}
