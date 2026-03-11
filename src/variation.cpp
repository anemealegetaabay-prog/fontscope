#include "fontscope/variation.h"

namespace fontscope {

static constexpr uint8_t kDeltaRunZeroes = 0x80;
static constexpr uint8_t kDeltaRunWords  = 0x40;
static constexpr uint8_t kDeltaCountMask = 0x3F;

static bool read_packed_deltas(ByteReader& r, uint16_t n_pts,
                                std::vector<int16_t>& out)
{
    out.clear();
    out.reserve(n_pts);

    while (out.size() < n_pts) {
        uint8_t ctrl = r.read_u8();
        if (!r.ok()) return false;
        uint8_t count = (ctrl & kDeltaCountMask) + 1;

        if (ctrl & kDeltaRunZeroes) {
            for (uint8_t i = 0; i < count && out.size() < n_pts; ++i)
                out.push_back(0);
        } else if (ctrl & kDeltaRunWords) {
            for (uint8_t i = 0; i < count && out.size() < n_pts; ++i) {
                out.push_back(r.read_i16_be());
                if (!r.ok()) return false;
            }
        } else {
            for (uint8_t i = 0; i < count && out.size() < n_pts; ++i) {
                out.push_back(r.read_i8());
                if (!r.ok()) return false;
            }
        }
    }
    return true;
}

Result<VariationStore> parse_gvar(ByteReader& r, uint16_t num_glyphs,
                                   uint16_t num_axes)
{
    uint16_t major = r.read_u16_be();
    uint16_t minor = r.read_u16_be();
    if (!r.ok()) return Result<VariationStore>::error(Status::TruncatedInput);
    if (major != 1)
        return Result<VariationStore>::error(Status::UnsupportedVersion);
    (void)minor;

    uint16_t n_axes   = r.read_u16_be();
    uint16_t n_shared = r.read_u16_be();
    uint32_t shared_tuples_offset = r.read_u32_be();
    uint32_t glyph_variation_data_offset = r.read_u32_be();
    uint16_t flags    = r.read_u16_be();
    uint16_t n_glyphs = r.read_u16_be();
    if (!r.ok()) return Result<VariationStore>::error(Status::TruncatedInput);

    (void)n_axes; (void)n_shared; (void)shared_tuples_offset;
    (void)glyph_variation_data_offset; (void)flags;

    if (n_glyphs != num_glyphs)
        return Result<VariationStore>::error(Status::MalformedTable);

    VariationStore store;
    store.glyph_data.resize(num_glyphs);

    // Simplified gvar: read each glyph's variation record in sequence.
    // (Production code would use the offset array; this subset is sufficient
    // for analysis and fuzzing of the delta application path.)
    for (uint16_t g = 0; g < num_glyphs; ++g) {
        GlyphVariationData& gvd = store.glyph_data[g];
        uint16_t n_regions = r.read_u16_be();
        uint16_t n_pts     = r.read_u16_be();
        if (!r.ok()) break;

        gvd.delta_sets.resize(n_regions);
        for (auto& ds : gvd.delta_sets) {
            ds.region_index = r.read_u16_be();
            if (!r.ok()) break;
            read_packed_deltas(r, n_pts, ds.deltas);
        }
    }

    return Result<VariationStore>::success(std::move(store));
}

Fixed16 region_scalar(const VariationRegion& region,
                      const std::vector<F2Dot14>& coords)
{
    Fixed16 scalar = Fixed16::from_int(1);

    for (size_t i = 0; i < region.axes.size() && i < coords.size(); ++i) {
        const auto& ax = region.axes[i];
        double peak  = ax.peak_coord.to_f64();
        double start = ax.start_coord.to_f64();
        double end   = ax.end_coord.to_f64();
        double val   = coords[i].to_f64();

        double factor;
        if (peak == 0.0) {
            factor = 1.0;
        } else if (val < start || val > end) {
            factor = 0.0;
        } else if (val == peak) {
            factor = 1.0;
        } else if (val < peak) {
            factor = (start == peak) ? 1.0 : (val - start) / (peak - start);
        } else {
            factor = (end == peak) ? 1.0 : (end - val) / (end - peak);
        }

        scalar = scalar * Fixed16::from_f64(factor);
    }

    return scalar;
}

} // namespace fontscope
