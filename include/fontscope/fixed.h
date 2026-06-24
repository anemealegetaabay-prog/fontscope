#pragma once
#include <cstdint>
#include <cmath>

namespace fontscope {

// 16.16 fixed-point value as used in OpenType (e.g. matrix components, version).
struct Fixed16 {
    int32_t raw;

    static Fixed16 from_raw(int32_t r) { return {r}; }
    static Fixed16 from_int(int32_t i) { return {int32_t(uint32_t(i) << 16)}; }
    static Fixed16 from_f64(double d)  { return {int32_t(d * 65536.0)}; }

    int32_t integer()  const { return raw >> 16; }
    int32_t fraction() const { return raw & 0xFFFF; }
    double  to_f64()   const { return double(raw) / 65536.0; }
    int32_t round()    const { return (raw + 0x8000) >> 16; }
    int32_t floor_i()  const { return raw >> 16; }
    int32_t ceil_i()   const { return (raw + 0xFFFF) >> 16; }

    Fixed16 operator+(Fixed16 o) const { return {raw + o.raw}; }
    Fixed16 operator-(Fixed16 o) const { return {raw - o.raw}; }
    Fixed16 operator-()          const { return {-raw}; }
    Fixed16 operator*(Fixed16 o) const {
        return {int32_t((int64_t(raw) * o.raw) >> 16)};
    }
    Fixed16 operator*(int32_t i) const { return {raw * i}; }
    bool operator==(Fixed16 o) const { return raw == o.raw; }
    bool operator!=(Fixed16 o) const { return raw != o.raw; }
    bool operator< (Fixed16 o) const { return raw < o.raw; }
    bool operator<=(Fixed16 o) const { return raw <= o.raw; }
    bool operator> (Fixed16 o) const { return raw > o.raw; }
    bool operator>=(Fixed16 o) const { return raw >= o.raw; }
};

// 2.14 fixed-point value as used in OpenType variation coords.
struct F2Dot14 {
    int16_t raw;

    static F2Dot14 from_raw(int16_t r) { return {r}; }
    static F2Dot14 from_f64(double d)  {
        int32_t v = int32_t(std::round(d * 16384.0));
        if (v >  16383) v =  16383;
        if (v < -16384) v = -16384;
        return {int16_t(v)};
    }

    double  to_f64()   const { return double(raw) / 16384.0; }
    bool operator==(F2Dot14 o) const { return raw == o.raw; }
};

Fixed16 fixed_div(Fixed16 a, Fixed16 b);
Fixed16 fixed_lerp(Fixed16 a, Fixed16 b, Fixed16 t);
Fixed16 fixed_clamp(Fixed16 v, Fixed16 lo, Fixed16 hi);
Fixed16 fixed_abs(Fixed16 v);
Fixed16 fixed_round_to_grid(Fixed16 v);
Fixed16 fixed_ceil_to_grid(Fixed16 v);
Fixed16 fixed_floor_to_grid(Fixed16 v);
Fixed16 fixed_frac(Fixed16 v);

// Conversion between 16.16 and 26.6 (TrueType pixel coordinates).
Fixed16 fixed_from_26dot6(int32_t v);
int32_t fixed_to_26dot6(Fixed16 v);

F2Dot14 f2dot14_mul(F2Dot14 a, F2Dot14 b);
F2Dot14 f2dot14_lerp(F2Dot14 a, F2Dot14 b, F2Dot14 t);

} // namespace fontscope
