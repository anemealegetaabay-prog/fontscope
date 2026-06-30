#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/font_types.h"

namespace fontscope {

// One glyph's bitmap record inside an sbix strike. `data` holds the raw
// embedded image bytes (PNG/JPEG/TIFF) exactly as stored in the font.
struct SbixGlyphData {
    int16_t              origin_offset_x{0};
    int16_t              origin_offset_y{0};
    Tag                  graphic_type{0};   // 'png ', 'jpg ', 'tiff', 'dupe'
    std::vector<uint8_t> data;

    bool empty() const { return data.empty(); }
};

// A single bitmap strike: all glyphs rendered at one ppem/ppi.
struct SbixStrike {
    uint16_t                   ppem{0};
    uint16_t                   ppi{0};
    std::vector<SbixGlyphData> glyphs;       // indexed by glyph id
};

// Parsed sbix table (Apple/OpenType embedded bitmap strikes).
struct SbixTable {
    uint16_t                version{0};
    uint16_t                flags{0};
    std::vector<SbixStrike> strikes;
};

// Parse the sbix table. `num_glyphs` comes from maxp and sizes each strike's
// per-glyph offset array.
Result<SbixTable> parse_sbix(ByteReader& r, uint16_t num_glyphs);

// Natural pixel dimensions of an sbix glyph's embedded image, decoded from the
// image header (PNG IHDR for 'png ' graphics).
struct SbixImageSize {
    uint16_t width;
    uint16_t height;
};
SbixImageSize sbix_image_size(const SbixGlyphData& g);

// Short human-readable name for a glyph's graphic type ("png", "jpeg", ...).
const char* sbix_graphic_type_name(const SbixGlyphData& g);

} // namespace fontscope
