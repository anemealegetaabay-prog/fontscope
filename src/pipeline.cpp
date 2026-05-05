#include "fontscope/pipeline.h"
#include "fontscope/kern.h"
#include "fontscope/sfnt.h"
#include <algorithm>

namespace fontscope {

Result<RenderResult> render_glyph_pipeline(const FontFace& font,
                                            uint16_t glyph_id,
                                            const PipelineConfig& cfg)
{
    LoadOptions opts;
    opts.ppem             = cfg.ppem;
    opts.apply_hints      = cfg.apply_hints;
    opts.apply_variation  = cfg.apply_variation;
    opts.apply_iup        = true;
    opts.normalized_coords= cfg.normalized_coords;

    HintContext ctx = make_hint_context(font, glyph_id, cfg.ppem);

    VariationStore vstore;
    const TableRecord* gvar_rec = find_table(font.sfnt, tags::GVAR());
    if (gvar_rec && cfg.apply_variation) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(gvar_rec->offset, gvar_rec->length);
        uint16_t n_axes = font.has_fvar ? uint16_t(font.fvar.axes.size()) : 0;
        auto gvar_res = parse_gvar(sub, font.maxp.num_glyphs, n_axes);
        if (gvar_res.ok()) vstore = std::move(gvar_res.value);
    }

    auto pg_res = load_and_process_glyph(font, ctx, vstore, glyph_id, opts);
    if (!pg_res.ok()) return Result<RenderResult>::error(pg_res.status);

    const ProcessedGlyph& pg = pg_res.value;

    RenderResult res;
    res.advance = scale_to_pixels(pg.hmetrics.advance_width, cfg.ppem,
                                   font.head.units_per_em);
    res.bearing = scale_to_pixels(pg.hmetrics.lsb, cfg.ppem,
                                   font.head.units_per_em);

    if (pg.is_empty || pg.points.empty()) {
        res.coverage.init(1, 1, 8);
        res.coverage.clear(0);
        return Result<RenderResult>::success(std::move(res));
    }

    if (cfg.antialiased) {
        render_glyph_aa(pg.points, pg.end_pts, font.head.units_per_em,
                        cfg.ppem, res.coverage);
    } else {
        render_glyph(pg.points, pg.end_pts, font.head.units_per_em,
                     cfg.ppem, res.coverage);
    }

    return Result<RenderResult>::success(std::move(res));
}

StringRenderResult render_string(const FontFace& font,
                                  const std::vector<uint32_t>& codepoints,
                                  const PipelineConfig& cfg)
{
    StringRenderResult result;
    result.ascender  = scale_to_pixels(font.hhea.ascender,  cfg.ppem,
                                        font.head.units_per_em);
    result.descender = scale_to_pixels(font.hhea.descender, cfg.ppem,
                                        font.head.units_per_em);

    // Load kern table once.
    KernTable kern;
    bool has_kern = false;
    const TableRecord* kern_rec = find_table(font.sfnt, tags::KERN());
    if (kern_rec) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(kern_rec->offset, kern_rec->length);
        auto kr = parse_kern(sub);
        if (kr.ok()) { kern = std::move(kr.value); has_kern = true; }
    }

    int32_t x_pen = 0;
    uint16_t prev_gid = 0;
    bool first = true;

    for (uint32_t cp : codepoints) {
        uint16_t gid = font.cmap.lookup(cp);
        if (gid == 0 && cp != 0x20) gid = 0;  // use notdef for unmapped

        // Apply kerning.
        if (!first && has_kern) {
            int16_t kern_val = kern.lookup(prev_gid, gid);
            if (kern_val != 0)
                x_pen += scale_to_pixels(kern_val, cfg.ppem, font.head.units_per_em);
        }

        result.x_positions.push_back(x_pen);

        auto rr = render_glyph_pipeline(font, gid, cfg);
        if (rr.ok()) {
            x_pen += rr.value.advance;
            result.glyphs.push_back(std::move(rr.value));
        } else {
            result.glyphs.push_back({});
            GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, gid);
            x_pen += scale_to_pixels(hm.advance_width, cfg.ppem,
                                      font.head.units_per_em);
        }

        prev_gid = gid;
        first    = false;
    }

    result.total_width = x_pen;
    return result;
}

Result<std::vector<uint8_t>> render_color_glyph(const FontFace& font,
                                                  uint16_t glyph_id,
                                                  const PipelineConfig& cfg)
{
    if (!font.has_colr || !font.has_cpal)
        return Result<std::vector<uint8_t>>::error(Status::TableNotFound);

    const ColorGlyph* cg = find_color_glyph(font.colr, glyph_id);
    if (!cg)
        return Result<std::vector<uint8_t>>::error(Status::GlyphNotFound);

    uint32_t w = cfg.ppem;
    uint32_t h = cfg.ppem;
    std::vector<uint8_t> rgba(w * h * 4, 0);

    for (uint16_t li = 0; li < cg->num_layers; ++li) {
        uint16_t layer_idx = cg->first_layer_index + li;
        if (layer_idx >= font.colr.layers.size()) break;

        const LayerRecord& layer = font.colr.layers[layer_idx];

        // Render the layer glyph to a coverage mask.
        auto rr = render_glyph_pipeline(font, layer.glyph_id, cfg);
        if (!rr.ok()) continue;

        // Look up the palette color.
        const ColorEntry* ce = lookup_color(font.cpal, cfg.palette_id,
                                             layer.palette_index);
        if (!ce) continue;

        uint32_t fg = (uint32_t(ce->alpha) << 24) |
                      (uint32_t(ce->red)   << 16) |
                      (uint32_t(ce->green) <<  8) |
                       uint32_t(ce->blue);

        composite_over(rr.value.coverage, fg, rgba.data(), w, h, 0, 0);
    }

    return Result<std::vector<uint8_t>>::success(std::move(rgba));
}

} // namespace fontscope
