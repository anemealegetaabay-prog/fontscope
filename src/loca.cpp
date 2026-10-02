#include "fontscope/loca.h"

namespace fontscope {

GlyfRange LocaTable::glyph_range(uint16_t glyph_id) const {
    if (size_t(glyph_id) + 1 >= offsets.size())
        return {0, 0};
    return {offsets[glyph_id], offsets[glyph_id + 1]};
}

Result<LocaTable> parse_loca(ByteReader& r, uint16_t num_glyphs,
                              int16_t index_to_loc_format)
{
    LocaTable loca;
    uint32_t count = uint32_t(num_glyphs) + 1;
    loca.offsets.reserve(count);

    if (index_to_loc_format == 0) {
        // Short format: Offset16 values, each multiplied by 2.
        for (uint32_t i = 0; i < count; ++i) {
            loca.offsets.push_back(uint32_t(r.read_u16_be()) * 2u);
            if (!r.ok()) return Result<LocaTable>::error(Status::TruncatedInput);
        }
    } else if (index_to_loc_format == 1) {
        // Long format: Offset32 values used directly.
        for (uint32_t i = 0; i < count; ++i) {
            loca.offsets.push_back(r.read_u32_be());
            if (!r.ok()) return Result<LocaTable>::error(Status::TruncatedInput);
        }
    } else {
        return Result<LocaTable>::error(Status::MalformedTable);
    }

    return Result<LocaTable>::success(std::move(loca));
}

} // namespace fontscope
