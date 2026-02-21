#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

// One BGRA color entry in a CPAL palette.
struct ColorEntry {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
};

struct Palette {
    std::vector<ColorEntry> colors;
};

struct CpalTable {
    uint16_t              version;
    uint16_t              num_palette_entries;
    std::vector<Palette>  palettes;
};

Result<CpalTable> parse_cpal(ByteReader& r);

// Returns nullptr if palette_index or palette_id is out of range.
const ColorEntry* lookup_color(const CpalTable& cpal, uint16_t palette_id,
                                uint16_t palette_index);

} // namespace fontscope
