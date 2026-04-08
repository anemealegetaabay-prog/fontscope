#include "fontscope/metrics.h"
#include <cstdio>
#include <algorithm>

namespace fontscope {

int16_t compute_rsb(const GlyphHMetrics& hm, FUnit x_min, FUnit x_max) {
    int32_t ink_width = int32_t(x_max) - int32_t(x_min);
    int32_t rsb = int32_t(hm.advance_width) - int32_t(hm.lsb) - ink_width;
    return int16_t(rsb);
}

std::vector<GlyphMetricsInfo> collect_all_metrics(const FontFace& font) {
    std::vector<GlyphMetricsInfo> result;
    result.reserve(font.maxp.num_glyphs);

    for (uint16_t gid = 0; gid < font.maxp.num_glyphs; ++gid) {
        GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, gid);

        GlyphMetricsInfo info{};
        info.glyph_id      = gid;
        info.advance_width = hm.advance_width;
        info.lsb           = hm.lsb;

        auto gr = load_glyph(font, gid);
        if (gr.ok() && !gr.value.is_empty()) {
            info.x_min = gr.value.x_min;
            info.y_min = gr.value.y_min;
            info.x_max = gr.value.x_max;
            info.y_max = gr.value.y_max;
            info.rsb   = compute_rsb(hm, gr.value.x_min, gr.value.x_max);
        }

        result.push_back(info);
    }
    return result;
}

void print_metrics_summary(const FontFace& font) {
    printf("Font metrics (hhea):\n");
    printf("  Ascender:      %d\n",  font.hhea.ascender);
    printf("  Descender:     %d\n",  font.hhea.descender);
    printf("  Line gap:      %d\n",  font.hhea.line_gap);
    printf("  Max adv width: %u\n",  font.hhea.advance_width_max);

    if (font.has_os2) {
        printf("Font metrics (OS/2):\n");
        printf("  Typo ascender:  %d\n", font.os2.s_typo_ascender);
        printf("  Typo descender: %d\n", font.os2.s_typo_descender);
        printf("  Typo line gap:  %d\n", font.os2.s_typo_line_gap);
        printf("  Win ascent:     %u\n", font.os2.us_win_ascent);
        printf("  Win descent:    %u\n", font.os2.us_win_descent);
        printf("  Weight class:   %u\n", font.os2.us_weight_class);
        printf("  Width class:    %u\n", font.os2.us_width_class);
    }
}

void print_glyph_advances(const FontFace& font, uint16_t from_gid, uint16_t to_gid) {
    uint16_t end = std::min(to_gid, uint16_t(font.maxp.num_glyphs - 1));
    printf("  GID   AdvWidth  LSB\n");
    for (uint16_t gid = from_gid; gid <= end; ++gid) {
        GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, gid);
        printf("  %5u  %7u  %d\n", gid, hm.advance_width, hm.lsb);
    }
}

OutlineStats collect_outline_stats(const FontFace& font) {
    OutlineStats s{};
    for (uint16_t gid = 0; gid < font.maxp.num_glyphs; ++gid) {
        auto gr = load_glyph(font, gid);
        if (!gr.ok()) continue;
        const RawGlyph& g = gr.value;
        if (g.is_empty()) { ++s.empty_glyphs; continue; }
        if (g.is_composite()) { ++s.composite_glyphs; continue; }

        uint32_t n_pts  = uint32_t(g.points.size());
        uint32_t n_cont = uint32_t(g.end_pts_of_contours.size());
        s.total_points   += n_pts;
        s.total_contours += n_cont;
        if (n_pts > s.max_points_per_glyph)  s.max_points_per_glyph  = n_pts;
        if (n_cont > s.max_contours_per_glyph) s.max_contours_per_glyph = n_cont;

        for (const auto& pt : g.points) {
            if (pt.on_curve) ++s.on_curve_points;
            else             ++s.off_curve_points;
        }
    }
    return s;
}

void print_outline_stats(const OutlineStats& s) {
    printf("Outline statistics:\n");
    printf("  Total points:          %u\n", s.total_points);
    printf("  Total contours:        %u\n", s.total_contours);
    printf("  On-curve points:       %u\n", s.on_curve_points);
    printf("  Off-curve points:      %u\n", s.off_curve_points);
    printf("  Composite glyphs:      %u\n", s.composite_glyphs);
    printf("  Empty glyphs:          %u\n", s.empty_glyphs);
    printf("  Max pts/glyph:         %u\n", s.max_points_per_glyph);
    printf("  Max contours/glyph:    %u\n", s.max_contours_per_glyph);
}

} // namespace fontscope
