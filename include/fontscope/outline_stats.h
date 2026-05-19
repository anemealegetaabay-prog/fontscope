#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/glyf.h"
#include "fontscope/font_types.h"

namespace fontscope {

// Per-glyph outline analysis.
struct GlyphOutlineInfo {
    uint16_t glyph_id;
    uint16_t n_contours;
    uint16_t n_points;
    uint16_t n_on_curve;
    uint16_t n_off_curve;
    uint16_t n_components;   // for composite glyphs
    int16_t  x_min, y_min, x_max, y_max;
    int32_t  ink_width;      // x_max - x_min
    int32_t  ink_height;     // y_max - y_min
    bool     is_composite;
    bool     is_empty;
    bool     has_instructions;
    uint16_t instruction_bytes;
};

// Font-wide outline statistics.
struct OutlineSummary {
    uint32_t total_glyphs;
    uint32_t simple_glyphs;
    uint32_t composite_glyphs;
    uint32_t empty_glyphs;
    uint32_t glyphs_with_instructions;

    uint64_t total_points;
    uint64_t total_contours;
    uint64_t total_on_curve;
    uint64_t total_off_curve;
    uint64_t total_instruction_bytes;

    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_components;
    uint32_t max_instruction_bytes;

    double   avg_points_per_glyph;
    double   avg_contours_per_glyph;
};

// Per-contour properties.
struct ContourInfo {
    uint16_t n_points;
    uint16_t n_on_curve;
    uint16_t n_off_curve;
    bool     is_clockwise;
    int32_t  signed_area_x2;  // twice the signed area from shoelace formula
};

// Analyze a single glyph's outline.
GlyphOutlineInfo analyze_glyph_outline(const RawGlyph& g, uint16_t glyph_id);

// Analyze all contours in a simple glyph.
std::vector<ContourInfo> analyze_contours(const RawGlyph& g);

// Compute a font-wide summary.
OutlineSummary summarize_outlines(const std::vector<GlyphOutlineInfo>& infos);

// Print a per-glyph table.
void print_outline_info(const GlyphOutlineInfo& info);
void print_outline_summary(const OutlineSummary& summary);

// Check if a glyph outline is self-intersecting (approximate, based on bbox overlaps).
bool outline_may_self_intersect(const RawGlyph& g);

// Compute the signed area of a contour (positive = counter-clockwise in Y-up).
int64_t contour_signed_area(const std::vector<FPoint>& pts,
                             uint16_t start, uint16_t end);

// Detect open contours (first point != last point after IUP).
bool contour_is_closed(const std::vector<FPoint>& pts,
                        uint16_t start, uint16_t end);

// Find the glyph with the most points in a font.
uint16_t find_most_complex_glyph(const std::vector<GlyphOutlineInfo>& infos);

// Return only glyphs whose ink area exceeds a threshold.
std::vector<uint16_t> filter_glyphs_by_ink(
    const std::vector<GlyphOutlineInfo>& infos,
    uint32_t min_ink_area);

} // namespace fontscope
