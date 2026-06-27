#include "fontscope/raster.h"
#include <cstdio>
#include <cassert>
#include <vector>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

// Contour on, off, off — previously caused flatten_contour to loop forever.
static void test_render_glyph_off_curve_wrap() {
    std::vector<FPoint> points = {
        {0,   0, true},
        {50, 100, false},
        {100, 0, false},
    };
    std::vector<uint16_t> end_pts = {2};

    RasterBuf buf;
    render_glyph(points, end_pts, 1000, 16, buf);
    CHECK(buf.width == 16);
    CHECK(buf.height == 16);
    CHECK(!buf.pixels.empty());
}

// Off-curve followed by on-curve near contour end (wraps to start).
static void test_render_glyph_off_then_on_wrap() {
    std::vector<FPoint> points = {
        {0,   0, true},
        {100, 0, true},
        {50,  50, false},
    };
    std::vector<uint16_t> end_pts = {2};

    RasterBuf buf;
    render_glyph(points, end_pts, 1000, 16, buf);
    CHECK(buf.width == 16);
    CHECK(!buf.pixels.empty());
}

// Large outline of off-curve points with wide coordinate swings — exercises the
// total-work budget so flattening stays bounded instead of exploding.
static void test_render_glyph_bounded_work() {
    std::vector<FPoint> points;
    points.push_back({0, 0, true});
    for (int i = 0; i < 4000; ++i) {
        bool on = (i % 2) == 0;
        FUnit x = FUnit((i * 9173) % 30000 - 15000);
        FUnit y = FUnit((i * 4271) % 30000 - 15000);
        points.push_back({x, y, on});
    }
    std::vector<uint16_t> end_pts = {uint16_t(points.size() - 1)};

    RasterBuf buf;
    render_glyph(points, end_pts, 1000, 16, buf);
    CHECK(buf.width == 16);
    CHECK(!buf.pixels.empty());
}

// Negative design coordinates must not trigger undefined left-shift in edge build.
static void test_render_glyph_negative_coords() {
    std::vector<FPoint> points = {
        {-500, -300, true},
        { 200, -100, true},
        { 100,  400, true},
    };
    std::vector<uint16_t> end_pts = {2};

    RasterBuf buf;
    render_glyph(points, end_pts, 1000, 16, buf);
    CHECK(buf.width == 16);
    CHECK(!buf.pixels.empty());
}

int main() {
    test_render_glyph_off_curve_wrap();
    test_render_glyph_off_then_on_wrap();
    test_render_glyph_bounded_work();
    test_render_glyph_negative_coords();
    if (failures) {
        fprintf(stderr, "test_raster: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_raster: all passed\n");
    return 0;
}
