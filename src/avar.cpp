#include "fontscope/avar.h"
#include <algorithm>

namespace fontscope {

Result<AvarTable> parse_avar(ByteReader& r) {
    AvarTable avar{};
    avar.major_version = r.read_u16_be();
    avar.minor_version = r.read_u16_be();
    r.skip(2);  // reserved
    uint16_t axis_count = r.read_u16_be();
    if (!r.ok()) return Result<AvarTable>::error(Status::TruncatedInput);

    avar.segment_maps.reserve(axis_count);
    for (uint16_t i = 0; i < axis_count; ++i) {
        uint16_t pos_count = r.read_u16_be();
        if (!r.ok()) break;

        AxisSegmentMap map;
        map.segments.reserve(pos_count);
        for (uint16_t p = 0; p < pos_count; ++p) {
            int16_t from = r.read_i16_be();
            int16_t to   = r.read_i16_be();
            if (!r.ok()) break;
            map.segments.push_back({F2Dot14::from_raw(from), F2Dot14::from_raw(to)});
        }
        avar.segment_maps.push_back(std::move(map));
    }

    return Result<AvarTable>::success(std::move(avar));
}

F2Dot14 apply_avar(const AxisSegmentMap& map, F2Dot14 coord) {
    const auto& segs = map.segments;
    if (segs.empty()) return coord;

    // Binary search for the enclosing segment.
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        if (coord.raw >= segs[i].from_coord.raw &&
            coord.raw <= segs[i+1].from_coord.raw)
        {
            int32_t from_range = int32_t(segs[i+1].from_coord.raw)
                               - int32_t(segs[i].from_coord.raw);
            int32_t to_range   = int32_t(segs[i+1].to_coord.raw)
                               - int32_t(segs[i].to_coord.raw);
            if (from_range == 0) return segs[i].to_coord;
            int32_t t = (int32_t(coord.raw) - int32_t(segs[i].from_coord.raw))
                      * to_range / from_range;
            return F2Dot14::from_raw(int16_t(int32_t(segs[i].to_coord.raw) + t));
        }
    }
    return coord;
}

} // namespace fontscope
