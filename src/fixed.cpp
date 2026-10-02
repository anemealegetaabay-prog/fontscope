#include "fontscope/fixed.h"
#include <cstdint>

namespace fontscope {

Fixed16 fixed_div(Fixed16 a, Fixed16 b) {
    if (b.raw == 0) return Fixed16::from_int(0);
    // Scale by 2^16 via multiply (not a left shift) so a negative dividend
    // does not invoke left-shift-of-negative UB.
    return Fixed16::from_raw(int32_t((int64_t(a.raw) * 65536) / b.raw));
}

Fixed16 fixed_lerp(Fixed16 a, Fixed16 b, Fixed16 t) {
    return a + (b - a) * t;
}

Fixed16 fixed_clamp(Fixed16 v, Fixed16 lo, Fixed16 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Fixed16 fixed_abs(Fixed16 v) {
    return v.raw < 0 ? Fixed16::from_raw(-v.raw) : v;
}

Fixed16 fixed_round_to_grid(Fixed16 v) {
    return Fixed16::from_int(v.round());
}

Fixed16 fixed_ceil_to_grid(Fixed16 v) {
    return Fixed16::from_int(v.ceil_i());
}

Fixed16 fixed_floor_to_grid(Fixed16 v) {
    return Fixed16::from_int(v.floor_i());
}

Fixed16 fixed_frac(Fixed16 v) {
    return Fixed16::from_raw(v.raw & 0xFFFF);
}

Fixed16 fixed_from_26dot6(int32_t v) {
    return Fixed16::from_raw(v << 10);
}

int32_t fixed_to_26dot6(Fixed16 v) {
    return v.raw >> 10;
}

F2Dot14 f2dot14_mul(F2Dot14 a, F2Dot14 b) {
    int32_t r = (int32_t(a.raw) * int32_t(b.raw) + 0x2000) >> 14;
    if (r >  16383) r =  16383;
    if (r < -16384) r = -16384;
    return F2Dot14::from_raw(int16_t(r));
}

F2Dot14 f2dot14_lerp(F2Dot14 a, F2Dot14 b, F2Dot14 t) {
    int32_t diff = int32_t(b.raw) - int32_t(a.raw);
    int32_t r    = int32_t(a.raw) + ((diff * int32_t(t.raw) + 0x2000) >> 14);
    return F2Dot14::from_raw(int16_t(r));
}

} // namespace fontscope
