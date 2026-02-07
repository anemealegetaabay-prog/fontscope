#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/font_types.h"

namespace fontscope {

// TrueType simple glyph flag bits.
static constexpr uint8_t kFlagOnCurve    = 0x01;
static constexpr uint8_t kFlagXShortVec  = 0x02;
static constexpr uint8_t kFlagYShortVec  = 0x04;
static constexpr uint8_t kFlagRepeat     = 0x08;
static constexpr uint8_t kFlagXSameOrPos = 0x10;
static constexpr uint8_t kFlagYSameOrPos = 0x20;
static constexpr uint8_t kFlagOverlapSimple = 0x40;

// TrueType composite component flags.
static constexpr uint16_t kCompArgsAreXYValues  = 0x0002;
static constexpr uint16_t kCompRoundXYToGrid    = 0x0004;
static constexpr uint16_t kCompWeHaveAScale     = 0x0008;
static constexpr uint16_t kCompMoreComponents   = 0x0020;
static constexpr uint16_t kCompWeHaveXYScale    = 0x0040;
static constexpr uint16_t kCompWeHaveA2x2       = 0x0080;
static constexpr uint16_t kCompWeHaveInstructions = 0x0100;
static constexpr uint16_t kCompUseMyMetrics     = 0x0200;
static constexpr uint16_t kCompOverlapCompound  = 0x0400;

struct CompositeComponent {
    uint16_t flags;
    uint16_t glyph_index;
    int32_t  arg1;   // offset or point index
    int32_t  arg2;
    Fixed16  xx, xy, yx, yy;  // transformation matrix (identity if no scale)
};

struct RawGlyph {
    int16_t  number_of_contours;  // negative → composite
    FUnit    x_min, y_min, x_max, y_max;

    // Simple glyph fields:
    std::vector<uint16_t> end_pts_of_contours;
    std::vector<uint8_t>  flags;
    std::vector<FPoint>   points;
    std::vector<uint8_t>  instructions;

    // Composite glyph fields:
    std::vector<CompositeComponent> components;

    bool is_composite() const { return number_of_contours < 0; }
    bool is_empty()     const { return number_of_contours == 0; }
};

Result<RawGlyph> parse_glyph(ByteReader& r);

// Apply IUP (interpolate untouched points) to finalize the glyph outline.
// This is called after hinting if the hinter did not touch all points.
void iup_interpolate(RawGlyph& glyph, const std::vector<bool>& touched_x,
                     const std::vector<bool>& touched_y);

} // namespace fontscope
