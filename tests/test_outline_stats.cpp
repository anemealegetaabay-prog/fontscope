#include "fontscope/outline_stats.h"
#include "fontscope/glyf.h"
#include <cstdio>
#include <cassert>
#include <cmath>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)
#define CHECK_NEAR(a,b,eps) CHECK(std::fabs(double(a)-double(b)) < (eps))

static RawGlyph make_simple_glyph(
    std::vector<uint16_t> ends, std::vector<FPoint> pts)
{
    RawGlyph g{};
    g.number_of_contours = int16_t(ends.size());
    g.x_min = g.y_min = g.x_max = g.y_max = 0;
    if (!pts.empty()) {
        g.x_min = pts[0].x; g.x_max = pts[0].x;
        g.y_min = pts[0].y; g.y_max = pts[0].y;
        for (const auto& p : pts) {
            if (p.x < g.x_min) g.x_min = p.x;
            if (p.x > g.x_max) g.x_max = p.x;
            if (p.y < g.y_min) g.y_min = p.y;
            if (p.y > g.y_max) g.y_max = p.y;
        }
    }
    g.end_pts_of_contours = ends;
    g.points = pts;
    return g;
}

static void test_analyze_empty_glyph() {
    RawGlyph g{};
    GlyphOutlineInfo info = analyze_glyph_outline(g, 0);
    CHECK(info.glyph_id == 0);
    CHECK(info.is_empty == true);
    CHECK(info.is_composite == false);
    CHECK(info.n_points == 0);
    CHECK(info.n_contours == 0);
}

static void test_analyze_simple_glyph() {
    RawGlyph g = make_simple_glyph(
        {2},
        {{0, 0, true}, {50, 100, false}, {100, 0, true}});
    g.instructions = {0x00, 0x01};

    GlyphOutlineInfo info = analyze_glyph_outline(g, 5);
    CHECK(info.glyph_id == 5);
    CHECK(info.is_empty == false);
    CHECK(info.is_composite == false);
    CHECK(info.n_contours == 1);
    CHECK(info.n_points == 3);
    CHECK(info.n_on_curve == 2);
    CHECK(info.n_off_curve == 1);
    CHECK(info.has_instructions == true);
    CHECK(info.instruction_bytes == 2);
    CHECK(info.ink_width == 100);
    CHECK(info.ink_height == 100);
}

static void test_analyze_multi_contour() {
    RawGlyph g = make_simple_glyph(
        {3, 7},
        { {0,0,true},{100,0,true},{100,100,true},{0,100,true},   // outer
          {20,20,true},{80,20,true},{80,80,true},{20,80,true} }); // inner
    g.x_min = 0; g.y_min = 0; g.x_max = 100; g.y_max = 100;

    GlyphOutlineInfo info = analyze_glyph_outline(g, 1);
    CHECK(info.n_contours == 2);
    CHECK(info.n_points == 8);
    CHECK(info.n_on_curve == 8);
    CHECK(info.n_off_curve == 0);
}

static void test_analyze_contours_winding() {
    // CCW square: area > 0.
    RawGlyph g = make_simple_glyph(
        {3},
        {{0,0,true},{100,0,true},{100,100,true},{0,100,true}});

    auto contours = analyze_contours(g);
    CHECK(contours.size() == 1);
    CHECK(contours[0].n_points == 4);
    // Shoelace area in TrueType Y-up: CCW = positive.
    CHECK(contours[0].signed_area_x2 > 0);
    CHECK(contours[0].is_clockwise == false);
}

static void test_analyze_contours_cw() {
    // CW square (reversed point order): area < 0.
    RawGlyph g = make_simple_glyph(
        {3},
        {{0,0,true},{0,100,true},{100,100,true},{100,0,true}});

    auto contours = analyze_contours(g);
    CHECK(contours.size() == 1);
    CHECK(contours[0].signed_area_x2 < 0);
    CHECK(contours[0].is_clockwise == true);
}

static void test_summarize_outlines() {
    std::vector<GlyphOutlineInfo> infos;

    // 2 simple, 1 composite, 1 empty.
    GlyphOutlineInfo a{};
    a.glyph_id = 1; a.is_empty = false; a.is_composite = false;
    a.n_points = 10; a.n_contours = 2;
    a.n_on_curve = 8; a.n_off_curve = 2;
    a.has_instructions = true; a.instruction_bytes = 12;

    GlyphOutlineInfo b{};
    b.glyph_id = 2; b.is_empty = false; b.is_composite = false;
    b.n_points = 20; b.n_contours = 3;
    b.n_on_curve = 18; b.n_off_curve = 2;

    GlyphOutlineInfo c{};
    c.glyph_id = 3; c.is_empty = false; c.is_composite = true;
    c.n_components = 2;

    GlyphOutlineInfo d{};
    d.glyph_id = 4; d.is_empty = true;

    infos = {a, b, c, d};
    OutlineSummary s = summarize_outlines(infos);

    CHECK(s.total_glyphs == 4);
    CHECK(s.simple_glyphs == 2);
    CHECK(s.composite_glyphs == 1);
    CHECK(s.empty_glyphs == 1);
    CHECK(s.total_points == 30);
    CHECK(s.total_contours == 5);
    CHECK(s.total_on_curve == 26);
    CHECK(s.total_off_curve == 4);
    CHECK(s.glyphs_with_instructions == 1);
    CHECK(s.total_instruction_bytes == 12);
    CHECK(s.max_points == 20);
    CHECK(s.max_contours == 3);
    CHECK_NEAR(s.avg_points_per_glyph, 15.0, 1e-5);
    CHECK_NEAR(s.avg_contours_per_glyph, 2.5, 1e-5);
}

static void test_contour_signed_area() {
    // Unit square CCW.
    std::vector<FPoint> pts = {
        {0, 0, true}, {1, 0, true}, {1, 1, true}, {0, 1, true}};
    int64_t area = contour_signed_area(pts, 0, 3);
    CHECK(area > 0);
    CHECK(area == 2);  // 2 * area = 2*1 = 2
}

static void test_outline_may_self_intersect() {
    // Two non-overlapping contours: no self-intersection.
    RawGlyph g = make_simple_glyph(
        {3, 7},
        { {0,0,true},{10,0,true},{10,10,true},{0,10,true},
          {20,20,true},{30,20,true},{30,30,true},{20,30,true}});
    CHECK(!outline_may_self_intersect(g));

    // Two overlapping contours: potential self-intersection.
    RawGlyph g2 = make_simple_glyph(
        {3, 7},
        { {0,0,true},{20,0,true},{20,20,true},{0,20,true},
          {10,10,true},{30,10,true},{30,30,true},{10,30,true}});
    CHECK(outline_may_self_intersect(g2));
}

static void test_find_most_complex_glyph() {
    std::vector<GlyphOutlineInfo> infos;
    GlyphOutlineInfo a{}; a.glyph_id = 1; a.n_points = 10;
    GlyphOutlineInfo b{}; b.glyph_id = 2; b.n_points = 50;
    GlyphOutlineInfo c{}; c.glyph_id = 3; c.n_points = 30;
    infos = {a, b, c};
    CHECK(find_most_complex_glyph(infos) == 2);
}

static void test_filter_glyphs_by_ink() {
    std::vector<GlyphOutlineInfo> infos;
    GlyphOutlineInfo a{}; a.glyph_id = 1; a.ink_width = 10; a.ink_height = 10;
    GlyphOutlineInfo b{}; b.glyph_id = 2; b.ink_width = 100; b.ink_height = 100;
    GlyphOutlineInfo c{}; c.glyph_id = 3; c.is_empty = true;
    infos = {a, b, c};

    auto result = filter_glyphs_by_ink(infos, 5000);
    CHECK(result.size() == 1);
    CHECK(result[0] == 2);
}

int main() {
    test_analyze_empty_glyph();
    test_analyze_simple_glyph();
    test_analyze_multi_contour();
    test_analyze_contours_winding();
    test_analyze_contours_cw();
    test_summarize_outlines();
    test_contour_signed_area();
    test_outline_may_self_intersect();
    test_find_most_complex_glyph();
    test_filter_glyphs_by_ink();

    if (failures) {
        fprintf(stderr, "test_outline_stats: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_outline_stats: all passed\n");
    return 0;
}
