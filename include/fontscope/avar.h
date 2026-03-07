#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/fixed.h"

namespace fontscope {

struct SegmentMapEntry {
    F2Dot14 from_coord;
    F2Dot14 to_coord;
};

struct AxisSegmentMap {
    std::vector<SegmentMapEntry> segments;
};

struct AvarTable {
    uint16_t                    major_version;
    uint16_t                    minor_version;
    std::vector<AxisSegmentMap> segment_maps;
};

Result<AvarTable> parse_avar(ByteReader& r);

// Apply an avar segment map to remap a normalized coordinate.
F2Dot14 apply_avar(const AxisSegmentMap& map, F2Dot14 coord);

} // namespace fontscope
