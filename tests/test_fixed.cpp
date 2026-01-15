#include "fontscope/fixed.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace fontscope;

static void test_fixed16_basic() {
    Fixed16 a = Fixed16::from_int(3);
    Fixed16 b = Fixed16::from_int(2);
    assert((a + b).integer() == 5);
    assert((a - b).integer() == 1);
    assert((a * b).integer() == 6);
    assert(a.integer() == 3);
    assert(a.round() == 3);
}

static void test_fixed16_fraction() {
    Fixed16 v = Fixed16::from_f64(1.5);
    assert(v.integer() == 1);
    assert(v.round() == 2);
    assert(std::fabs(v.to_f64() - 1.5) < 0.0001);
}

static void test_fixed16_negative() {
    Fixed16 v = Fixed16::from_int(-4);
    assert(v.integer() == -4);
    assert((-v).integer() == 4);
    Fixed16 neg_half = Fixed16::from_f64(-0.5);
    assert(neg_half.round() == 0);
    assert(neg_half.ceil_i() == 0);
    assert(neg_half.floor_i() == -1);
}

static void test_fixed_div() {
    Fixed16 a = Fixed16::from_int(10);
    Fixed16 b = Fixed16::from_int(4);
    Fixed16 r = fixed_div(a, b);
    assert(std::fabs(r.to_f64() - 2.5) < 0.001);
}

static void test_fixed_clamp() {
    Fixed16 lo = Fixed16::from_int(0);
    Fixed16 hi = Fixed16::from_int(10);
    assert(fixed_clamp(Fixed16::from_int(-1), lo, hi).integer() == 0);
    assert(fixed_clamp(Fixed16::from_int(5),  lo, hi).integer() == 5);
    assert(fixed_clamp(Fixed16::from_int(15), lo, hi).integer() == 10);
}

static void test_fixed_abs() {
    assert(fixed_abs(Fixed16::from_int(-7)).integer() == 7);
    assert(fixed_abs(Fixed16::from_int(3)).integer()  == 3);
}

static void test_fixed_lerp() {
    Fixed16 a = Fixed16::from_int(0);
    Fixed16 b = Fixed16::from_int(10);
    Fixed16 t = Fixed16::from_f64(0.5);
    Fixed16 r = fixed_lerp(a, b, t);
    assert(std::fabs(r.to_f64() - 5.0) < 0.1);
}

static void test_26dot6_roundtrip() {
    Fixed16 v = Fixed16::from_f64(3.25);
    int32_t px = fixed_to_26dot6(v);
    Fixed16 back = fixed_from_26dot6(px);
    assert(std::fabs(back.to_f64() - 3.25) < 0.02);
}

static void test_f2dot14() {
    F2Dot14 a = F2Dot14::from_f64(1.0);
    assert(std::fabs(a.to_f64() - 1.0) < 0.001);

    F2Dot14 b = F2Dot14::from_f64(-1.0);
    assert(std::fabs(b.to_f64() - (-1.0)) < 0.001);

    F2Dot14 half = F2Dot14::from_f64(0.5);
    assert(std::fabs(half.to_f64() - 0.5) < 0.001);

    // Clamp at ±1.0.
    F2Dot14 over = F2Dot14::from_f64(2.0);
    assert(std::fabs(over.to_f64() - 1.0) < 0.001);
}

static void test_f2dot14_mul() {
    F2Dot14 a = F2Dot14::from_f64(0.5);
    F2Dot14 b = F2Dot14::from_f64(0.5);
    F2Dot14 r = f2dot14_mul(a, b);
    assert(std::fabs(r.to_f64() - 0.25) < 0.002);
}

int main() {
    test_fixed16_basic();
    test_fixed16_fraction();
    test_fixed16_negative();
    test_fixed_div();
    test_fixed_clamp();
    test_fixed_abs();
    test_fixed_lerp();
    test_26dot6_roundtrip();
    test_f2dot14();
    test_f2dot14_mul();
    puts("test_fixed: all passed");
    return 0;
}
