#pragma once
#include <cstdint>
#include <cstring>
#include "fontscope/fixed.h"

namespace fontscope {

// Font design-space unit (1/unitsPerEm of an em).
using FUnit = int16_t;

// A single outline point in font design space.
struct FPoint {
    FUnit x;
    FUnit y;
    bool  on_curve;
};

// Axis-aligned bounding box in design-space units.
struct FBBox {
    FUnit x_min, y_min, x_max, y_max;
};

// 4-byte OpenType table tag, stored as big-endian uint32.
struct Tag {
    uint32_t value;

    static Tag make(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        return {uint32_t(a) << 24 | uint32_t(b) << 16 | uint32_t(c) << 8 | d};
    }
    static Tag from_chars(const char s[4]) {
        return make(uint8_t(s[0]), uint8_t(s[1]), uint8_t(s[2]), uint8_t(s[3]));
    }
    bool operator==(Tag o) const { return value == o.value; }
    bool operator!=(Tag o) const { return value != o.value; }
};

// Per-glyph horizontal advance metrics from hmtx.
struct GlyphHMetrics {
    uint16_t advance_width;
    int16_t  lsb;
};

// Glyph metrics in device pixels after scaling.
struct ScaledMetrics {
    int32_t  advance;
    int32_t  bearing;
    int32_t  ascender;
    int32_t  descender;
};

namespace tags {
    inline Tag HEAD() { return Tag::from_chars("head"); }
    inline Tag MAXP() { return Tag::from_chars("maxp"); }
    inline Tag HHEA() { return Tag::from_chars("hhea"); }
    inline Tag HMTX() { return Tag::from_chars("hmtx"); }
    inline Tag POST() { return Tag::from_chars("post"); }
    inline Tag NAME() { return Tag::from_chars("name"); }
    inline Tag OS2()  { return Tag::make('O','S','/','2'); }
    inline Tag CMAP() { return Tag::from_chars("cmap"); }
    inline Tag LOCA() { return Tag::from_chars("loca"); }
    inline Tag GLYF() { return Tag::from_chars("glyf"); }
    inline Tag FPGM() { return Tag::from_chars("fpgm"); }
    inline Tag PREP() { return Tag::from_chars("prep"); }
    inline Tag CVT()  { return Tag::make('c','v','t',' '); }
    inline Tag COLR() { return Tag::make('C','O','L','R'); }
    inline Tag CPAL() { return Tag::make('C','P','A','L'); }
    inline Tag FVAR() { return Tag::from_chars("fvar"); }
    inline Tag AVAR() { return Tag::from_chars("avar"); }
    inline Tag GVAR() { return Tag::from_chars("gvar"); }
    inline Tag CVAR() { return Tag::from_chars("cvar"); }
    inline Tag KERN() { return Tag::from_chars("kern"); }
    inline Tag GSUB() { return Tag::from_chars("GSUB"); }
    inline Tag GPOS() { return Tag::from_chars("GPOS"); }
    inline Tag CFF()  { return Tag::make('C','F','F',' '); }
    inline Tag SBIX() { return Tag::from_chars("sbix"); }
}

} // namespace fontscope
