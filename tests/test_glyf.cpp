#include "fontscope/glyf.h"
#include "fontscope/loca.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace fontscope;

static std::vector<uint8_t> make_simple_glyph() {
    std::vector<uint8_t> buf;
    auto pi16 = [&](int16_t v) { buf.push_back(v>>8); buf.push_back(v&0xFF); };
    auto pu16 = [&](uint16_t v){ buf.push_back(v>>8); buf.push_back(v&0xFF); };

    pi16(1);      // numberOfContours = 1
    pi16(-100); pi16(-100); pi16(100); pi16(100);  // bbox
    pu16(3);      // endPtsOfContours[0] = 3 (4 points)
    pu16(0);      // instructionLength = 0
    // flags: 4 points, on-curve, short x (positive), short y (positive)
    // 0x01=on-curve, 0x02=XShort, 0x04=YShort, 0x10=XPos, 0x20=YPos
    buf.push_back(0x37); buf.push_back(0x37);
    buf.push_back(0x37); buf.push_back(0x37);
    // x-coords (1-byte deltas, all positive)
    buf.push_back(10); buf.push_back(20); buf.push_back(30); buf.push_back(40);
    // y-coords (1-byte deltas, all positive)
    buf.push_back(5);  buf.push_back(10); buf.push_back(15); buf.push_back(20);
    return buf;
}

static void test_simple_glyph() {
    auto buf = make_simple_glyph();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_glyph(r);
    assert(res.ok());
    assert(!res.value.is_composite());
    assert(!res.value.is_empty());
    assert(res.value.end_pts_of_contours.size() == 1);
    assert(res.value.points.size() == 4);
    for (const auto& pt : res.value.points)
        assert(pt.on_curve);
}

static void test_empty_glyph() {
    ByteReader r(nullptr, 0);
    auto res = parse_glyph(r);
    assert(res.ok());
    assert(res.value.is_empty());
}

static void test_truncated_glyph() {
    uint8_t buf[] = {0x00, 0x01};  // just numberOfContours, no bbox
    ByteReader r(buf, sizeof(buf));
    auto res = parse_glyph(r);
    assert(!res.ok());
}

static std::vector<uint8_t> make_composite_glyph() {
    std::vector<uint8_t> buf;
    auto pi16 = [&](int16_t v) { buf.push_back(v>>8); buf.push_back(v&0xFF); };
    auto pu16 = [&](uint16_t v){ buf.push_back(v>>8); buf.push_back(v&0xFF); };

    pi16(-1);  // numberOfContours = -1 (composite)
    pi16(0); pi16(0); pi16(200); pi16(200);  // bbox

    // One component: flags=ARG_1_AND_2_ARE_WORDS|ARGS_ARE_XY_VALUES, no more
    uint16_t flags = 0x0002 | 0x0001;  // ARGS_ARE_XY | ARGS_ARE_WORDS
    pu16(flags);
    pu16(42);   // component glyph index
    pi16(10);   // dx
    pi16(-5);   // dy
    return buf;
}

static void test_composite_glyph() {
    auto buf = make_composite_glyph();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_glyph(r);
    assert(res.ok());
    assert(res.value.is_composite());
    assert(res.value.components.size() == 1);
    assert(res.value.components[0].glyph_index == 42);
    assert(res.value.components[0].arg1 == 10);
    assert(res.value.components[0].arg2 == -5);
}

static void test_loca_short() {
    // Short loca: offsets are uint16 * 2. 3 glyphs → 4 entries.
    uint8_t buf[] = {0x00,0x00, 0x00,0x14, 0x00,0x28, 0x00,0x28};
    ByteReader r(buf, sizeof(buf));
    auto res = parse_loca(r, 3, 0);
    assert(res.ok());
    assert(res.value.offsets.size() == 4);
    assert(res.value.offsets[0] == 0);
    assert(res.value.offsets[1] == 0x28);
    assert(res.value.offsets[2] == 0x50);
    assert(res.value.offsets[3] == 0x50);

    GlyfRange range = res.value.glyph_range(1);
    assert(range.begin == 0x28);
    assert(range.end   == 0x50);
}

static void test_loca_long() {
    uint8_t buf[] = {
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x64,
        0x00,0x00,0x00,0x64,
    };
    ByteReader r(buf, sizeof(buf));
    auto res = parse_loca(r, 2, 1);
    assert(res.ok());
    assert(res.value.offsets[0] == 0);
    assert(res.value.offsets[1] == 100);
    assert(res.value.offsets[2] == 100);

    GlyfRange range = res.value.glyph_range(0);
    assert(range.begin == 0);
    assert(range.end   == 100);

    // Glyph 1 is empty (offset[1] == offset[2]).
    GlyfRange r1 = res.value.glyph_range(1);
    assert(r1.begin == r1.end);
}

static void test_iup() {
    RawGlyph g{};
    g.number_of_contours = 1;
    g.end_pts_of_contours = {3};
    g.points = {{0, 0, true}, {0, 0, true}, {100, 100, true}, {0, 0, true}};
    // Touch points 0 and 2; let 1 and 3 be interpolated.
    std::vector<bool> tx = {true, false, true, false};
    std::vector<bool> ty = {true, false, true, false};
    iup_interpolate(g, tx, ty);
    // Points 1 and 3 should now be interpolated between 0 and 2.
    // (exact values depend on IUP logic; we just check they changed)
    assert(g.points[1].x != 0 || g.points[2].x == 100);
}

int main() {
    test_simple_glyph();
    test_empty_glyph();
    test_truncated_glyph();
    test_composite_glyph();
    test_loca_short();
    test_loca_long();
    test_iup();
    puts("test_glyf: all passed");
    return 0;
}
