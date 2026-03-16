#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/font_types.h"
#include "fontscope/variation.h"

namespace fontscope {

struct PointDelta {
    int32_t dx;
    int32_t dy;
};

// Compute the net per-point variation deltas for a glyph at the given
// normalized design-space coordinates.
std::vector<PointDelta> compute_point_deltas(
    const std::vector<VariationRegion>& regions,
    const GlyphVariationData& gvd,
    const std::vector<F2Dot14>& coords,
    uint16_t n_points);

// Apply pre-computed deltas to an outline's point array in-place.
void apply_deltas(std::vector<FPoint>& points,
                  const std::vector<PointDelta>& deltas);

} // namespace fontscope
