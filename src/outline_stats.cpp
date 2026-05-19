#include "fontscope/outline_stats.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace fontscope {

int64_t contour_signed_area(const std::vector<FPoint>& pts,
                              uint16_t start, uint16_t end)
{
    int64_t area = 0;
    for (uint16_t i = start; i <= end; ++i) {
        uint16_t j = (i == end) ? start : i + 1;
        area += int64_t(pts[i].x) * int64_t(pts[j].y)
              - int64_t(pts[j].x) * int64_t(pts[i].y);
    }
    return area;  // positive = CCW (Y-up)
}

bool contour_is_closed(const std::vector<FPoint>& pts,
                        uint16_t start, uint16_t end)
{
    if (start >= pts.size() || end >= pts.size() || start > end) return false;
    return pts[start].x == pts[end].x && pts[start].y == pts[end].y;
}

GlyphOutlineInfo analyze_glyph_outline(const RawGlyph& g, uint16_t glyph_id) {
    GlyphOutlineInfo info{};
    info.glyph_id   = glyph_id;
    info.is_empty   = g.is_empty();
    info.is_composite = g.is_composite();

    info.x_min = g.x_min;
    info.y_min = g.y_min;
    info.x_max = g.x_max;
    info.y_max = g.y_max;
    info.ink_width  = int32_t(g.x_max) - int32_t(g.x_min);
    info.ink_height = int32_t(g.y_max) - int32_t(g.y_min);

    if (g.is_composite()) {
        info.n_components = uint16_t(g.components.size());
        return info;
    }

    if (!g.is_empty()) {
        info.n_contours = uint16_t(g.end_pts_of_contours.size());
        info.n_points   = uint16_t(g.points.size());

        for (const auto& p : g.points) {
            if (p.on_curve) ++info.n_on_curve;
            else            ++info.n_off_curve;
        }

        info.has_instructions  = !g.instructions.empty();
        info.instruction_bytes = uint16_t(g.instructions.size());
    }

    return info;
}

std::vector<ContourInfo> analyze_contours(const RawGlyph& g) {
    std::vector<ContourInfo> result;
    if (g.is_empty() || g.is_composite()) return result;

    uint16_t start = 0;
    for (uint16_t ep : g.end_pts_of_contours) {
        if (ep >= g.points.size() || ep < start) { start = ep+1; continue; }

        ContourInfo ci{};
        ci.n_points = uint16_t(ep - start + 1);
        for (uint16_t i = start; i <= ep; ++i) {
            if (g.points[i].on_curve) ++ci.n_on_curve;
            else                       ++ci.n_off_curve;
        }

        ci.signed_area_x2 = int32_t(contour_signed_area(g.points, start, ep));
        ci.is_clockwise   = ci.signed_area_x2 < 0;

        result.push_back(ci);
        start = ep + 1;
    }
    return result;
}

OutlineSummary summarize_outlines(const std::vector<GlyphOutlineInfo>& infos) {
    OutlineSummary s{};
    s.total_glyphs = uint32_t(infos.size());

    for (const auto& g : infos) {
        if (g.is_empty)     { ++s.empty_glyphs; continue; }
        if (g.is_composite) { ++s.composite_glyphs; continue; }

        ++s.simple_glyphs;
        s.total_points   += g.n_points;
        s.total_contours += g.n_contours;
        s.total_on_curve += g.n_on_curve;
        s.total_off_curve += g.n_off_curve;

        if (g.has_instructions) {
            ++s.glyphs_with_instructions;
            s.total_instruction_bytes += g.instruction_bytes;
        }

        if (g.n_points   > s.max_points)   s.max_points   = g.n_points;
        if (g.n_contours > s.max_contours) s.max_contours = g.n_contours;
        if (g.n_components > s.max_components) s.max_components = g.n_components;
        if (g.instruction_bytes > s.max_instruction_bytes)
            s.max_instruction_bytes = g.instruction_bytes;
    }

    if (s.simple_glyphs > 0) {
        s.avg_points_per_glyph   = double(s.total_points)   / s.simple_glyphs;
        s.avg_contours_per_glyph = double(s.total_contours) / s.simple_glyphs;
    }

    return s;
}

void print_outline_info(const GlyphOutlineInfo& info) {
    if (info.is_empty) {
        printf("  %5u: empty\n", info.glyph_id);
        return;
    }
    if (info.is_composite) {
        printf("  %5u: composite %u components\n",
               info.glyph_id, info.n_components);
        return;
    }
    printf("  %5u: %2u contours, %4u pts (%u on/%u off), bbox (%d,%d)-(%d,%d)%s\n",
           info.glyph_id,
           info.n_contours, info.n_points,
           info.n_on_curve, info.n_off_curve,
           info.x_min, info.y_min, info.x_max, info.y_max,
           info.has_instructions ? " *" : "");
}

void print_outline_summary(const OutlineSummary& s) {
    printf("Outline summary:\n");
    printf("  Total glyphs:           %u\n", s.total_glyphs);
    printf("  Simple glyphs:          %u\n", s.simple_glyphs);
    printf("  Composite glyphs:       %u\n", s.composite_glyphs);
    printf("  Empty glyphs:           %u\n", s.empty_glyphs);
    printf("  Glyphs with hints:      %u\n", s.glyphs_with_instructions);
    printf("  Total points:           %llu\n", (unsigned long long)s.total_points);
    printf("  Total contours:         %llu\n", (unsigned long long)s.total_contours);
    printf("  On-curve:               %llu\n", (unsigned long long)s.total_on_curve);
    printf("  Off-curve:              %llu\n", (unsigned long long)s.total_off_curve);
    printf("  Total instruction bytes:%llu\n", (unsigned long long)s.total_instruction_bytes);
    printf("  Max points/glyph:       %u\n", s.max_points);
    printf("  Max contours/glyph:     %u\n", s.max_contours);
    printf("  Max components/glyph:   %u\n", s.max_components);
    printf("  Avg points/glyph:       %.1f\n", s.avg_points_per_glyph);
    printf("  Avg contours/glyph:     %.1f\n", s.avg_contours_per_glyph);
}

bool outline_may_self_intersect(const RawGlyph& g) {
    if (g.is_empty() || g.is_composite() || g.end_pts_of_contours.size() < 2)
        return false;

    // Check if any two contour bounding boxes overlap.
    struct BBox { int16_t x0, y0, x1, y1; };
    std::vector<BBox> boxes;

    uint16_t start = 0;
    for (uint16_t ep : g.end_pts_of_contours) {
        if (ep >= g.points.size()) break;
        BBox bb = {32767, 32767, -32768, -32768};
        for (uint16_t i = start; i <= ep; ++i) {
            bb.x0 = std::min(bb.x0, g.points[i].x);
            bb.y0 = std::min(bb.y0, g.points[i].y);
            bb.x1 = std::max(bb.x1, g.points[i].x);
            bb.y1 = std::max(bb.y1, g.points[i].y);
        }
        boxes.push_back(bb);
        start = ep + 1;
    }

    for (size_t i = 0; i < boxes.size(); ++i) {
        for (size_t j = i+1; j < boxes.size(); ++j) {
            const BBox& a = boxes[i];
            const BBox& b = boxes[j];
            if (a.x0 <= b.x1 && a.x1 >= b.x0 &&
                a.y0 <= b.y1 && a.y1 >= b.y0)
                return true;
        }
    }
    return false;
}

uint16_t find_most_complex_glyph(const std::vector<GlyphOutlineInfo>& infos) {
    uint16_t best = 0;
    uint16_t max_pts = 0;
    for (const auto& g : infos) {
        if (g.n_points > max_pts) {
            max_pts = g.n_points;
            best    = g.glyph_id;
        }
    }
    return best;
}

std::vector<uint16_t> filter_glyphs_by_ink(
    const std::vector<GlyphOutlineInfo>& infos,
    uint32_t min_ink_area)
{
    std::vector<uint16_t> result;
    for (const auto& g : infos) {
        if (g.is_empty || g.is_composite) continue;
        uint32_t area = uint32_t(g.ink_width) * uint32_t(g.ink_height);
        if (area >= min_ink_area) result.push_back(g.glyph_id);
    }
    return result;
}

} // namespace fontscope
