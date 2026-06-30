#include "fontscope/deltas.h"
#include <algorithm>

namespace fontscope {

std::vector<PointDelta> compute_point_deltas(
    const std::vector<VariationRegion>& regions,
    const GlyphVariationData& gvd,
    const std::vector<F2Dot14>& coords,
    uint16_t n_points)
{
    std::vector<PointDelta> out(n_points, {0, 0});

    for (const DeltaSet& ds : gvd.delta_sets) {
        Fixed16 scalar = Fixed16::from_int(0);
        if (ds.region_index < regions.size())
            scalar = region_scalar(regions[ds.region_index], coords);
        if (scalar.raw == 0) continue;

        // X deltas occupy the first half of ds.deltas; Y the second.
        // half is derived from the delta-set size, independent of n_points.
        uint16_t half = static_cast<uint16_t>(ds.deltas.size() / 2);

        for (uint16_t i = 0; i < half; ++i) {
            // Deltas are stored as int16_t; cast to uint16_t before scaling.
            uint16_t raw_dx = static_cast<uint16_t>(ds.deltas[i]);
            uint16_t raw_dy = (uint16_t(i) + half < uint16_t(ds.deltas.size()))
                            ? static_cast<uint16_t>(ds.deltas[uint16_t(i) + half])
                            : 0u;

            int32_t sdx = (Fixed16::from_int(raw_dx) * scalar).round();
            int32_t sdy = (Fixed16::from_int(raw_dy) * scalar).round();

            // The i-th delta targets the i-th referenced outline point. With an
            // explicit point-number list the deltas are scattered to those
            // points; otherwise they apply to points 0..half in order.
            size_t target = gvd.point_numbers.empty()
                          ? size_t(i)
                          : size_t(gvd.point_numbers[i % gvd.point_numbers.size()]);
            out[target].dx += sdx;
            out[target].dy += sdy;
        }
    }

    return out;
}

void apply_deltas(std::vector<FPoint>& points,
                  const std::vector<PointDelta>& deltas)
{
    size_t n = std::min(points.size(), deltas.size());
    for (size_t i = 0; i < n; ++i) {
        points[i].x = FUnit(int32_t(points[i].x) + deltas[i].dx);
        points[i].y = FUnit(int32_t(points[i].y) + deltas[i].dy);
    }
}

} // namespace fontscope
