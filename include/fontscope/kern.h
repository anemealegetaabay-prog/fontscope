#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

// A single kerning pair (left glyph, right glyph → value in FUnits).
struct KernPair {
    uint16_t left;
    uint16_t right;
    int16_t  value;
};

// One kern subtable (format 0: sorted pair list).
struct KernSubtable {
    uint16_t            version;
    uint16_t            coverage;  // bit 0: horizontal, bit 1: minimum, bit 2: cross-stream
    std::vector<KernPair> pairs;
};

struct KernTable {
    std::vector<KernSubtable> subtables;

    // Return the adjustment (in FUnits) for the pair (left, right).
    // Returns 0 if no pair is found.
    int16_t lookup(uint16_t left, uint16_t right) const;
};

Result<KernTable> parse_kern(ByteReader& r);

} // namespace fontscope
