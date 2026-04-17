#include "fontscope/fvar.h"
#include "fontscope/avar.h"
#include "fontscope/variation.h"
#include "fontscope/deltas.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace fontscope;

static void test_normalize_axis() {
    VariationAxis ax;
    ax.min_value     = Fixed16::from_int(-100);
    ax.default_value = Fixed16::from_int(0);
    ax.max_value     = Fixed16::from_int(100);

    F2Dot14 at_max = normalize_axis(ax, Fixed16::from_int(100));
    assert(std::fabs(at_max.to_f64() - 1.0) < 0.01);

    F2Dot14 at_min = normalize_axis(ax, Fixed16::from_int(-100));
    assert(std::fabs(at_min.to_f64() - (-1.0)) < 0.01);

    F2Dot14 at_def = normalize_axis(ax, Fixed16::from_int(0));
    assert(at_def.raw == 0);
}

static void test_normalize_axis_clamped() {
    VariationAxis ax;
    ax.min_value     = Fixed16::from_int(0);
    ax.default_value = Fixed16::from_int(0);
    ax.max_value     = Fixed16::from_int(100);

    F2Dot14 over = normalize_axis(ax, Fixed16::from_int(200));
    assert(std::fabs(over.to_f64() - 1.0) < 0.01);
}

static void test_apply_avar_identity() {
    AxisSegmentMap map;
    map.segments = {
        {F2Dot14::from_f64(-1.0), F2Dot14::from_f64(-1.0)},
        {F2Dot14::from_raw(0),    F2Dot14::from_raw(0)},
        {F2Dot14::from_f64(1.0),  F2Dot14::from_f64(1.0)},
    };
    F2Dot14 in  = F2Dot14::from_f64(0.5);
    F2Dot14 out = apply_avar(map, in);
    assert(std::fabs(out.to_f64() - 0.5) < 0.01);
}

static void test_region_scalar_at_peak() {
    VariationRegion region;
    RegionAxisCoords ax;
    ax.start_coord = F2Dot14::from_f64(-1.0);
    ax.peak_coord  = F2Dot14::from_f64( 1.0);
    ax.end_coord   = F2Dot14::from_f64( 1.0);
    region.axes.push_back(ax);

    std::vector<F2Dot14> coords = {F2Dot14::from_f64(1.0)};
    Fixed16 sc = region_scalar(region, coords);
    assert(std::fabs(sc.to_f64() - 1.0) < 0.01);
}

static void test_region_scalar_outside() {
    VariationRegion region;
    RegionAxisCoords ax;
    ax.start_coord = F2Dot14::from_f64(0.0);
    ax.peak_coord  = F2Dot14::from_f64(0.5);
    ax.end_coord   = F2Dot14::from_f64(1.0);
    region.axes.push_back(ax);

    std::vector<F2Dot14> coords = {F2Dot14::from_f64(-0.5)};
    Fixed16 sc = region_scalar(region, coords);
    assert(sc.raw == 0);
}

static void test_compute_deltas_zero_scalar() {
    GlyphVariationData gvd;
    DeltaSet ds;
    ds.region_index = 999;  // out of range → scalar = 0
    ds.deltas = {100, 200, 50, 75};
    gvd.delta_sets.push_back(ds);

    std::vector<VariationRegion> regions;
    std::vector<F2Dot14> coords;
    auto out = compute_point_deltas(regions, gvd, coords, 2);
    assert(out.size() == 2);
    assert(out[0].dx == 0 && out[0].dy == 0);
}

static void test_apply_deltas_basic() {
    std::vector<FPoint> pts = {{10, 20, true}, {30, 40, true}};
    std::vector<PointDelta> deltas = {{5, -3}, {0, 10}};
    apply_deltas(pts, deltas);
    assert(pts[0].x == 15);
    assert(pts[0].y == 17);
    assert(pts[1].y == 50);
}

static void test_apply_deltas_fewer() {
    std::vector<FPoint> pts = {{1, 2, true}, {3, 4, true}, {5, 6, true}};
    std::vector<PointDelta> deltas = {{10, 10}};
    apply_deltas(pts, deltas);
    assert(pts[0].x == 11);
    assert(pts[1].x == 3);
}

int main() {
    test_normalize_axis();
    test_normalize_axis_clamped();
    test_apply_avar_identity();
    test_region_scalar_at_peak();
    test_region_scalar_outside();
    test_compute_deltas_zero_scalar();
    test_apply_deltas_basic();
    test_apply_deltas_fewer();
    puts("test_variation: all passed");
    return 0;
}
