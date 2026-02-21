#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

// One color layer entry: glyph ID + palette index.
struct LayerRecord {
    uint16_t glyph_id;
    uint16_t palette_index;
};

// A color glyph entry from the COLR BaseGlyphRecord array.
struct ColorGlyph {
    uint16_t glyph_id;
    uint16_t first_layer_index;
    uint16_t num_layers;
};

struct ColrTable {
    uint16_t                version;
    std::vector<ColorGlyph> glyphs;
    std::vector<LayerRecord> layers;
};

Result<ColrTable> parse_colr(ByteReader& r);
const ColorGlyph* find_color_glyph(const ColrTable& colr, uint16_t glyph_id);

} // namespace fontscope
