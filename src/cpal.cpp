#include "fontscope/cpal.h"

namespace fontscope {

Result<CpalTable> parse_cpal(ByteReader& r) {
    CpalTable cpal{};
    cpal.version             = r.read_u16_be();
    cpal.num_palette_entries = r.read_u16_be();
    uint16_t num_palettes    = r.read_u16_be();
    uint16_t num_colors      = r.read_u16_be();
    uint32_t color_records_offset = r.read_u32_be();
    if (!r.ok()) return Result<CpalTable>::error(Status::TruncatedInput);

    // colorRecordIndices: one uint16 per palette.
    std::vector<uint16_t> palette_starts(num_palettes);
    for (auto& v : palette_starts) {
        v = r.read_u16_be();
        if (!r.ok()) return Result<CpalTable>::error(Status::TruncatedInput);
    }

    // Read the flat ColorRecord array.
    ByteReader cr = r.sub_reader(color_records_offset,
                                  r.size() - color_records_offset);
    std::vector<ColorEntry> all_colors;
    all_colors.reserve(num_colors);
    for (uint16_t i = 0; i < num_colors; ++i) {
        ColorEntry e{};
        e.blue  = cr.read_u8();
        e.green = cr.read_u8();
        e.red   = cr.read_u8();
        e.alpha = cr.read_u8();
        if (!cr.ok()) break;
        all_colors.push_back(e);
    }

    // Build per-palette views.
    cpal.palettes.resize(num_palettes);
    for (uint16_t p = 0; p < num_palettes; ++p) {
        uint16_t start = palette_starts[p];
        cpal.palettes[p].colors.reserve(cpal.num_palette_entries);
        for (uint16_t e = 0; e < cpal.num_palette_entries; ++e) {
            if (start + e < all_colors.size())
                cpal.palettes[p].colors.push_back(all_colors[start + e]);
        }
    }

    return Result<CpalTable>::success(std::move(cpal));
}

const ColorEntry* lookup_color(const CpalTable& cpal, uint16_t palette_id,
                                uint16_t palette_index)
{
    if (palette_id >= cpal.palettes.size()) return nullptr;
    const auto& pal = cpal.palettes[palette_id];
    if (palette_index >= pal.colors.size()) return nullptr;
    return &pal.colors[palette_index];
}

} // namespace fontscope
