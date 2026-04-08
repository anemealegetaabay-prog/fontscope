#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/inspect.h"

namespace fontscope {

struct GlyphMetricsInfo {
    uint16_t glyph_id;
    uint16_t advance_width;
    int16_t  lsb;
    int16_t  rsb;
    int16_t  x_min, y_min, x_max, y_max;
};

// Compute RSB for a glyph: RSB = advance_width - lsb - (x_max - x_min).
int16_t compute_rsb(const GlyphHMetrics& hm, FUnit x_min, FUnit x_max);

// Collect metrics for all glyphs in the font.
std::vector<GlyphMetricsInfo> collect_all_metrics(const FontFace& font);

// Print a metrics summary to stdout.
void print_metrics_summary(const FontFace& font);

// Print per-glyph advance widths for a range of glyphs.
void print_glyph_advances(const FontFace& font, uint16_t from_gid, uint16_t to_gid);

struct OutlineStats {
    uint32_t total_points;
    uint32_t total_contours;
    uint32_t on_curve_points;
    uint32_t off_curve_points;
    uint32_t composite_glyphs;
    uint32_t empty_glyphs;
    uint32_t max_points_per_glyph;
    uint32_t max_contours_per_glyph;
};

// Iterate all glyphs and collect outline statistics.
OutlineStats collect_outline_stats(const FontFace& font);
void print_outline_stats(const OutlineStats& s);

} // namespace fontscope
