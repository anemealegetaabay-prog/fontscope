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

    (void)num_axes; (void)shared_tuples_offset;
    (void)glyph_variation_data_offset; (void)flags;

    if (n_glyphs != num_glyphs)
        return Result<VariationStore>::error(Status::MalformedTable);

    VariationStore store;
    store.glyph_data.resize(num_glyphs);

    // Shared region list: n_shared tuples, each n_axes peak coordinates. Each
    // region is given an inclusive interval around the default position so it
    // has non-zero influence between the default and its peak.
    store.regions.resize(n_shared);
    for (auto& region : store.regions) {
        region.axes.resize(n_axes);
        for (auto& ax : region.axes) {
            int16_t peak = r.read_i16_be();
            ax.peak_coord  = F2Dot14::from_raw(peak);
            ax.start_coord = F2Dot14::from_raw(peak < 0 ? peak : 0);
            ax.end_coord   = F2Dot14::from_raw(peak > 0 ? peak : 0);
        }
        if (!r.ok()) return Result<VariationStore>::error(Status::TruncatedInput);
    }

    // Shared point-number list: the set of outline points every glyph's deltas
    // target, stored once and referenced by all glyph variation records.
    std::vector<uint16_t> shared_points;
    uint16_t shared_point_count = r.read_u16_be();
    if (!r.ok()) return Result<VariationStore>::error(Status::TruncatedInput);
    shared_points.reserve(shared_point_count);
    for (uint16_t i = 0; i < shared_point_count; ++i) {
        shared_points.push_back(r.read_u16_be());
        if (!r.ok()) return Result<VariationStore>::error(Status::TruncatedInput);
    }

    // Per-glyph variation records: a delta set per region, plus the shared
    // point numbers that the deltas apply to.
    for (uint16_t g = 0; g < num_glyphs; ++g) {
        GlyphVariationData& gvd = store.glyph_data[g];
        gvd.point_numbers = shared_points;

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
