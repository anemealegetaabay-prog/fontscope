#include "fontscope/pipeline.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace fontscope;

namespace {

void pu16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x & 0xFF));
}

constexpr uint16_t kUpem = 1000;

// A FontFace whose color glyph (gid 2) has a single COLR layer: glyph 1, a
// filled rectangle `width_units` wide and one em tall, painted with palette
// entry 0. The tables the color renderer reads are filled in directly, so no
// font file is needed.
FontFace make_color_font(uint16_t width_units) {
    // glyf data for glyph 1: one contour of four on-curve points with 16-bit
    // coordinate deltas.
    std::vector<uint8_t> glyph;
    pu16(glyph, 1);                                   // numberOfContours
    pu16(glyph, 0); pu16(glyph, 0);                   // xMin, yMin
    pu16(glyph, width_units); pu16(glyph, kUpem);     // xMax, yMax
    pu16(glyph, 3);                                   // endPtsOfContours[0]
    pu16(glyph, 0);                                   // instructionLength
    for (int i = 0; i < 4; ++i) glyph.push_back(0x01);  // ON_CURVE_POINT
    const int16_t dx[4] = {0, 0, int16_t(width_units), 0};
    const int16_t dy[4] = {0, int16_t(kUpem), 0, int16_t(-int16_t(kUpem))};
    for (int16_t d : dx) pu16(glyph, uint16_t(d));
    for (int16_t d : dy) pu16(glyph, uint16_t(d));

    FontFace f{};
    f.raw_data = glyph;  // the buffer holds only the glyf table
    TableRecord glyf{};
    glyf.tag    = tags::GLYF();
    glyf.offset = 0;
    glyf.length = uint32_t(glyph.size());
    f.sfnt.tables.push_back(glyf);

    f.head.units_per_em = kUpem;
    f.maxp.num_glyphs   = 3;
    // Glyphs 0 and 2 are empty; glyph 1 is the rectangle.
    f.loca.offsets = {0, 0, uint32_t(glyph.size()), uint32_t(glyph.size())};
    f.hmtx.hmetrics = {{kUpem, 0}, {width_units, 0}, {kUpem, 0}};

    f.colr.glyphs = {{2, 0, 1}};
    f.colr.layers = {{1, 0}};
    f.has_colr = true;
    f.cpal.palettes = {Palette{{ColorEntry{0x30, 0x20, 0x10, 0xFF}}}};
    f.has_cpal = true;
    return f;
}

PipelineConfig config(uint16_t ppem) {
    PipelineConfig cfg;
    cfg.ppem        = ppem;
    cfg.antialiased = false;
    cfg.apply_hints = false;
    return cfg;
}

}  // namespace

// A layer whose advance (2 em = 32 px at 16 ppem) is wider than the canvas
// used to grow the canvas vector while blend() kept writing through the
// pointer it had cached before the resize: a heap-use-after-free in
// composite_over() under ASan. The canvas now stays ppem x ppem (the size
// callers assume, since only the bytes are returned) and the layer is clipped.
static void test_color_layer_wider_than_ppem_is_clipped() {
    const uint16_t ppem = 16;
    FontFace font = make_color_font(2 * kUpem);
    auto res = render_color_glyph(font, 2, config(ppem));
    assert(res.ok());
    assert(res.value.size() == size_t(ppem) * ppem * 4);
}

// A layer that fits the canvas renders at the same size.
static void test_color_layer_within_ppem() {
    const uint16_t ppem = 16;
    FontFace font = make_color_font(kUpem);
    auto res = render_color_glyph(font, 2, config(ppem));
    assert(res.ok());
    assert(res.value.size() == size_t(ppem) * ppem * 4);
}

int main() {
    test_color_layer_wider_than_ppem_is_clipped();
    test_color_layer_within_ppem();
    std::puts("test_color_render: all passed");
    return 0;
}
