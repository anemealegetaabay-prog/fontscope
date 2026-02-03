#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

struct GlyfRange {
    uint32_t begin;  // byte offset from glyf table start
    uint32_t end;    // exclusive
};

struct LocaTable {
    std::vector<uint32_t> offsets;  // length = num_glyphs + 1

    GlyfRange glyph_range(uint16_t glyph_id) const;
};

// index_to_loc_format: 0 = Offset16 (short), 1 = Offset32 (long).
Result<LocaTable> parse_loca(ByteReader& r, uint16_t num_glyphs,
                              int16_t index_to_loc_format);

} // namespace fontscope
