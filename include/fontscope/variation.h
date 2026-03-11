#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/fixed.h"
#include "fontscope/font_types.h"

namespace fontscope {

// One variation axis interval [start, peak, end] in normalized 2.14 coords.
struct RegionAxisCoords {
    F2Dot14 start_coord;
    F2Dot14 peak_coord;
    F2Dot14 end_coord;
};

struct VariationRegion {
    std::vector<RegionAxisCoords> axes;
};

// A single delta-set: applies scalar-weighted deltas for one region.
struct DeltaSet {
    uint16_t             region_index;
    std::vector<int16_t> deltas;  // interleaved: x-deltas then y-deltas
};

struct GlyphVariationData {
    std::vector<DeltaSet>    delta_sets;
    std::vector<uint16_t>    point_numbers;  // empty = all points
};

struct VariationStore {
    std::vector<VariationRegion>    regions;
    std::vector<GlyphVariationData> glyph_data;
};

// Parse the gvar table.
Result<VariationStore> parse_gvar(ByteReader& r, uint16_t num_glyphs,
                                   uint16_t num_axes);

// Compute the scalar influence of a variation region at the given design-space
// normalized coordinates.
Fixed16 region_scalar(const VariationRegion& region,
                      const std::vector<F2Dot14>& coords);

} // namespace fontscope
