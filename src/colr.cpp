#include "fontscope/colr.h"
#include <algorithm>

namespace fontscope {

Result<ColrTable> parse_colr(ByteReader& r) {
    ColrTable colr{};
    colr.version          = r.read_u16_be();
    uint16_t num_glyphs   = r.read_u16_be();
    uint32_t bgr_offset   = r.read_u32_be();  // BaseGlyphRecord offset
    uint32_t lr_offset    = r.read_u32_be();  // LayerRecord offset
    uint16_t num_layers   = r.read_u16_be();
    if (!r.ok()) return Result<ColrTable>::error(Status::TruncatedInput);

    // Parse BaseGlyphRecord array.
    ByteReader bgr = r.sub_reader(bgr_offset, r.size() - bgr_offset);
    colr.glyphs.reserve(num_glyphs);
    for (uint16_t i = 0; i < num_glyphs; ++i) {
        ColorGlyph g{};
        g.glyph_id          = bgr.read_u16_be();
        g.first_layer_index = bgr.read_u16_be();
        g.num_layers        = bgr.read_u16_be();
        if (!bgr.ok()) break;
        colr.glyphs.push_back(g);
    }

    // Parse LayerRecord array.
    ByteReader lr = r.sub_reader(lr_offset, r.size() - lr_offset);
    colr.layers.reserve(num_layers);
    for (uint16_t i = 0; i < num_layers; ++i) {
        LayerRecord rec{};
        rec.glyph_id      = lr.read_u16_be();
        rec.palette_index = lr.read_u16_be();
        if (!lr.ok()) break;
        colr.layers.push_back(rec);
    }

    return Result<ColrTable>::success(std::move(colr));
}

const ColorGlyph* find_color_glyph(const ColrTable& colr, uint16_t glyph_id) {
    for (const auto& g : colr.glyphs)
        if (g.glyph_id == glyph_id) return &g;
    return nullptr;
}

} // namespace fontscope
