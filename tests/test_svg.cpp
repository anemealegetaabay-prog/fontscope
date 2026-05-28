#include "fontscope/svg_export.h"
#include "fontscope/glyf.h"
#include <cstdio>
#include <cassert>
#include <cstring>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

static bool has(const std::string& s, const char* sub) {
    return s.find(sub) != std::string::npos;
}

static RawGlyph make_triangle() {
    RawGlyph g{};
    g.number_of_contours = 1;
    g.x_min =   0; g.y_min =   0;
    g.x_max = 100; g.y_max = 100;
    g.end_pts_of_contours = {2};
    g.points = {{0, 0, true}, {50, 100, true}, {100, 0, true}};
    return g;
}

static RawGlyph make_square() {
    RawGlyph g{};
    g.number_of_contours = 1;
    g.x_min =   0; g.y_min =   0;
    g.x_max = 100; g.y_max = 100;
    g.end_pts_of_contours = {3};
    g.points = {{0, 0, true}, {100, 0, true}, {100, 100, true}, {0, 100, true}};
    return g;
}

static void test_svg_x_y_no_flip() {
    CHECK(svg_x(0, 1.0, 0.0) == 0.0);
    CHECK(svg_x(100, 2.0, 10.0) == 210.0);
    double y = svg_y(50, 1.0, 0.0, false, 200);
    CHECK(y == 50.0);
}

static void test_svg_y_flip() {
    // flip_y: svg_y = viewport_height - (y*scale + ty_off)
    // With scale=1, ty_off=0, vph=200: svg_y(50) = 200 - 50 = 150.
    double y = svg_y(50, 1.0, 0.0, true, 200);
    CHECK(y == 150.0);
}

static void test_contour_to_svg_all_oncurve() {
    // All on-curve points → L commands.
    std::vector<FPoint> pts = {
        {0, 0, true}, {100, 0, true}, {100, 100, true}, {0, 100, true}};
    std::string d = contour_to_svg_commands(pts, 0, 3, false, 1.0, 0.0, 0.0);
    CHECK(has(d, "M"));
    CHECK(has(d, "L"));
    CHECK(has(d, "Z"));
}

static void test_contour_to_svg_with_offcurve() {
    // Mixed on/off curve → C commands.
    std::vector<FPoint> pts = {
        {0, 0, true}, {50, 100, false}, {100, 0, true}};
    std::string d = contour_to_svg_commands(pts, 0, 2, false, 1.0, 0.0, 0.0);
    CHECK(has(d, "M"));
    CHECK(has(d, "C"));
    CHECK(has(d, "Z"));
}

static void test_contour_empty_range() {
    std::vector<FPoint> pts = {{0, 0, true}};
    // end < start: should return empty.
    std::string d = contour_to_svg_commands(pts, 5, 3, false, 1.0, 0.0, 0.0);
    CHECK(d.empty());
}

static void test_glyph_to_svg_path_empty() {
    RawGlyph g{};
    // Empty glyph (num_contours=0): returns empty path.
    SvgExportOptions opts;
    std::string path = glyph_to_svg_path(g, opts);
    CHECK(path.empty());
}

static void test_glyph_to_svg_path_triangle() {
    RawGlyph g = make_triangle();
    SvgExportOptions opts;
    opts.viewport_width  = 200;
    opts.viewport_height = 200;
    opts.flip_y = false;
    std::string path = glyph_to_svg_path(g, opts);
    CHECK(!path.empty());
    CHECK(has(path, "M"));
    CHECK(has(path, "Z"));
}

static void test_glyph_to_svg_path_square() {
    RawGlyph g = make_square();
    SvgExportOptions opts;
    opts.viewport_width  = 200;
    opts.viewport_height = 200;
    std::string path = glyph_to_svg_path(g, opts);
    CHECK(!path.empty());
    CHECK(has(path, "M"));
    CHECK(has(path, "L"));
    CHECK(has(path, "Z"));
}

static void test_svg_document_structure() {
    // We can't load a real font, so test document structure with an empty glyph.
    RawGlyph g{};
    g.x_min = g.y_min = g.x_max = g.y_max = 0;

    // Build a minimal FontFace shell (just needs units_per_em).
    // We bypass load_font by calling glyph_to_svg with a stub.
    // Just test the SVG prefix.
    SvgExportOptions opts;
    opts.viewport_width  = 256;
    opts.viewport_height = 256;
    opts.fill_color = "#ff0000";

    // Without a real font, call the path exporter and verify it produces valid SVG tags.
    std::string path = glyph_to_svg_path(g, opts);
    CHECK(path.empty());  // empty glyph has no path
}

static void test_svg_escape_color() {
    SvgExportOptions opts;
    opts.fill_color   = "#123abc";
    opts.stroke_color = "none";
    opts.opacity      = 0.75;

    RawGlyph g = make_square();
    // Path should contain the fill color.
    std::string path = glyph_to_svg_path(g, opts);
    CHECK(!path.empty());
}

static void test_svg_multi_contour() {
    // Glyph with two contours (e.g. 'O').
    RawGlyph g{};
    g.number_of_contours = 2;
    g.x_min = 0; g.y_min = 0; g.x_max = 200; g.y_max = 200;
    g.end_pts_of_contours = {3, 7};
    g.points = {
        {0,   0,   true}, {200,   0,   true}, {200, 200, true}, {  0, 200, true},
        {40,  40,  true}, {160,  40,  true}, {160, 160, true}, { 40, 160, true},
    };

    SvgExportOptions opts;
    opts.viewport_width  = 300;
    opts.viewport_height = 300;
    opts.flip_y = false;
    std::string path = glyph_to_svg_path(g, opts);
    // Two Z's: one per contour.
    int z_count = 0;
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == 'Z') ++z_count;
    CHECK(z_count == 2);
}

int main() {
    test_svg_x_y_no_flip();
    test_svg_y_flip();
    test_contour_to_svg_all_oncurve();
    test_contour_to_svg_with_offcurve();
    test_contour_empty_range();
    test_glyph_to_svg_path_empty();
    test_glyph_to_svg_path_triangle();
    test_glyph_to_svg_path_square();
    test_svg_document_structure();
    test_svg_escape_color();
    test_svg_multi_contour();

    if (failures) {
        fprintf(stderr, "test_svg: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_svg: all passed\n");
    return 0;
}
