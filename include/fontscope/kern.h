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

// A glyph-class assignment table (format 2 sub-table): glyphs in
// [first_glyph, first_glyph + classes.size()) map to the listed class value,
// all other glyphs map to class 0.
struct KernClassTable {
    uint16_t              first_glyph{0};
    std::vector<uint16_t> classes;   // already pre-multiplied per the spec
};

// One kern subtable. Format 0 uses the sorted `pairs` list; format 2 uses the
// class-based `row_width` / class tables / `array` triple.
struct KernSubtable {
    uint16_t            version;
    uint16_t            coverage;  // bit 0: horizontal, bit 1: minimum, bit 2: cross-stream
    uint8_t             format{0};
    std::vector<KernPair> pairs;

    // Format 2 class kerning state.
    uint16_t            row_width{0};         // bytes per row in `array`
    KernClassTable      left_class;
    KernClassTable      right_class;
    std::vector<int16_t> array;               // flattened left x right value grid
};

struct KernTable {
    std::vector<KernSubtable> subtables;

    // Return the adjustment (in FUnits) for the pair (left, right).
    // Returns 0 if no pair is found.
    int16_t lookup(uint16_t left, uint16_t right) const;
};

Result<KernTable> parse_kern(ByteReader& r);

} // namespace fontscope
