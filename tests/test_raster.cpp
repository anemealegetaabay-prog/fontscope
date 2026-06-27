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

int main() {
    test_render_glyph_off_curve_wrap();
    test_render_glyph_off_then_on_wrap();
    if (failures) {
        fprintf(stderr, "test_raster: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_raster: all passed\n");
    return 0;
}
