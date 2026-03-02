#include "fontscope/colr.h"
#include "fontscope/cpal.h"
#include "fontscope/bitmap.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace fontscope;

static std::vector<uint8_t> make_colr() {
    std::vector<uint8_t> buf;
    auto pu16 = [&](uint16_t v){ buf.push_back(v>>8); buf.push_back(v); };
    auto pu32 = [&](uint32_t v){ buf.push_back(v>>24); buf.push_back(v>>16);
                                  buf.push_back(v>>8); buf.push_back(v); };

    // version=0, numBaseGlyphs=1, offsetBaseGlyphRecord=14, offsetLayerRecord=20, numLayers=2
    pu16(0);    // version
    pu16(1);    // numBaseGlyphs
    pu32(14);   // BaseGlyphRecord offset (relative to table start byte 0)
    pu32(20);   // LayerRecord offset
    pu16(2);    // numLayerRecords

    // BaseGlyphRecord at offset 14: glyphID=5, firstLayerIndex=0, numLayers=2
    pu16(5);  pu16(0);  pu16(2);

    // Two LayerRecords at offset 20:
    pu16(10); pu16(0);  // layer 0: glyph=10, palette_index=0
    pu16(11); pu16(1);  // layer 1: glyph=11, palette_index=1

    return buf;
}

static void test_colr_parse() {
    auto buf = make_colr();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_colr(r);
    assert(res.ok());
    assert(res.value.glyphs.size() == 1);
    assert(res.value.glyphs[0].glyph_id == 5);
    assert(res.value.glyphs[0].num_layers == 2);
    assert(res.value.layers.size() == 2);
    assert(res.value.layers[0].glyph_id == 10);
    assert(res.value.layers[1].palette_index == 1);
}

static void test_find_color_glyph() {
    auto buf = make_colr();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_colr(r);
    assert(res.ok());

    const ColorGlyph* found = find_color_glyph(res.value, 5);
    assert(found != nullptr);
    assert(found->first_layer_index == 0);

    const ColorGlyph* nf = find_color_glyph(res.value, 99);
    assert(nf == nullptr);
}

static std::vector<uint8_t> make_cpal() {
    std::vector<uint8_t> buf;
    auto pu16 = [&](uint16_t v){ buf.push_back(v>>8); buf.push_back(v); };
    auto pu32 = [&](uint32_t v){ buf.push_back(v>>24); buf.push_back(v>>16);
                                  buf.push_back(v>>8); buf.push_back(v); };

    // Header: 2+2+2+2+4=12 bytes. One palette start uint16 = 14 bytes total before color records.
    // version=0, numPaletteEntries=2, numPalettes=1, numColorRecords=2, colorRecordsOffset=14
    pu16(0); pu16(2); pu16(1); pu16(2); pu32(14);
    pu16(0);  // palette 0 starts at color record index 0

    // Two BGRA color records.
    buf.push_back(0x00); buf.push_back(0xFF); buf.push_back(0x00); buf.push_back(0xFF); // green
    buf.push_back(0xFF); buf.push_back(0x00); buf.push_back(0x00); buf.push_back(0xFF); // blue
    return buf;
}

static void test_cpal_parse() {
    auto buf = make_cpal();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_cpal(r);
    assert(res.ok());
    assert(res.value.palettes.size() == 1);
    assert(res.value.palettes[0].colors.size() == 2);
    assert(res.value.palettes[0].colors[0].green == 0xFF);
    assert(res.value.palettes[0].colors[1].blue  == 0xFF);
}

static void test_lookup_color() {
    auto buf = make_cpal();
    ByteReader r(buf.data(), buf.size());
    auto res = parse_cpal(r);
    assert(res.ok());

    const ColorEntry* ce = lookup_color(res.value, 0, 0);
    assert(ce != nullptr);
    assert(ce->green == 0xFF);

    const ColorEntry* oob = lookup_color(res.value, 0, 5);
    assert(oob == nullptr);
}

static void test_bitmap_render() {
    // 4x1 bitmap, 1bpp: bits 1010 → pixels 1,0,1,0
    uint8_t bits[] = {0xA0};  // 10100000b
    GlyphBitmap bm = make_bitmap(4, 1, 1, bits, sizeof(bits));
    assert(bm.width == 4 && bm.height == 1 && bm.depth == 1);

    std::vector<uint8_t> rgba(4 * 4, 0);
    ColorEntry ce{0, 0, 255, 255};  // red fully opaque
    render_bitmap_to_rgba(bm, ce, rgba.data(), 4, 1);
    // Pixel 0 (bit=1): alpha = 255
    assert(rgba[3] == 255);
    // Pixel 1 (bit=0): alpha = 0
    assert(rgba[7] == 0);
}

static void test_bitmap_stride_single_row() {
    // 8x1 bitmap, 1bpp: 8 pixels in 1 byte
    uint8_t bits[] = {0xFF};
    GlyphBitmap bm = make_bitmap(8, 1, 1, bits, sizeof(bits));

    uint8_t dst[8] = {};
    ColorEntry ce{0, 0, 0, 255};
    // blend row 0 — should not read past the 1-byte buffer
    blend_bitmap_row(bm, 0, dst, 8, ce, 255);
    // All pixels lit: each dst byte gets red=0, but no crash is the key test.
}

int main() {
    test_colr_parse();
    test_find_color_glyph();
    test_cpal_parse();
    test_lookup_color();
    test_bitmap_render();
    test_bitmap_stride_single_row();
    puts("test_colr: all passed");
    return 0;
}
