#include "fontscope/glyph_loader.h"
#include "fontscope/sfnt.h"
#include "fontscope/glyf.h"
#include <cstdio>
#include <vector>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

// Forged cvt table length must not reserve gigabytes of heap.
static void test_make_hint_context_cvt_cap() {
    FontFace font{};
    font.raw_data.assign(64, 0);
    font.head.units_per_em = 1000;
    font.maxp.num_glyphs = 1;

    TableRecord cvt{};
    cvt.tag    = tags::CVT();
    cvt.offset = 0;
    cvt.length = 0xFFFFFFFFu;  // forged huge length
    font.sfnt.tables.push_back(cvt);

    HintContext ctx = make_hint_context(font, 0, 16);
    CHECK(ctx.cvt.size() <= 32);  // only bytes actually present in raw_data
}

static void test_make_hint_context_storage_cap() {
    FontFace font{};
    font.raw_data.assign(16, 0);
    font.head.units_per_em = 1000;
    font.maxp.num_glyphs = 1;
    font.maxp.max_storage = 0xFFFF;

    HintContext ctx = make_hint_context(font, 0, 16);
    CHECK(ctx.storage.size() <= 4096);
}

// Composite glyph 2 = three copies of glyph 1 (a 100x100 square): the first
// placed by an XY offset, the next two by point matching (child point 0 onto
// parent point 2). The anchor point used to be kept as a pointer into the
// composite's point vector, which the second component's points are then
// appended to; once that reallocated, the third component read freed memory
// (heap-use-after-free under ASan).
static void test_point_matched_components_after_point_vector_grows() {
    std::vector<uint8_t> glyf;
    auto pu16 = [&](uint16_t v) { glyf.push_back(uint8_t(v >> 8)); glyf.push_back(uint8_t(v)); };

    // Glyph 1: simple square (0,0) (0,100) (100,100) (100,0).
    pu16(1);                          // numberOfContours
    pu16(0); pu16(0); pu16(100); pu16(100);
    pu16(3);                          // endPtsOfContours[0]
    pu16(0);                          // instructionLength
    for (int i = 0; i < 4; ++i) glyf.push_back(0x01);  // on-curve, 16-bit deltas
    for (int16_t d : {0, 0, 100, 0})    pu16(uint16_t(d));
    for (int16_t d : {0, 100, 0, -100}) pu16(uint16_t(d));
    const uint32_t square_end = uint32_t(glyf.size());

    // Glyph 2: composite of three glyph-1 components.
    pu16(0xFFFF);                     // numberOfContours = -1
    pu16(0); pu16(0); pu16(200); pu16(200);
    pu16(0x0001 | kCompArgsAreXYValues | kCompMoreComponents);
    pu16(1); pu16(0); pu16(0);        // glyph 1 at offset (0, 0)
    pu16(0x0001 | kCompMoreComponents);
    pu16(1); pu16(2); pu16(0);        // child point 0 onto parent point 2
    pu16(0x0001);
    pu16(1); pu16(2); pu16(0);        // again, reusing the anchor

    FontFace font{};
    font.raw_data = glyf;
    TableRecord rec{};
    rec.tag    = tags::GLYF();
    rec.offset = 0;
    rec.length = uint32_t(glyf.size());
    font.sfnt.tables.push_back(rec);
    font.head.units_per_em = 1000;
    font.maxp.num_glyphs   = 3;
    font.loca.offsets = {0, 0, square_end, uint32_t(glyf.size())};
    font.hmtx.hmetrics = {{1000, 0}, {1000, 0}, {1000, 0}};

    HintContext ctx = make_hint_context(font, 2, 16);
    VariationStore vstore;
    LoadOptions opts;
    opts.ppem        = 16;
    opts.apply_hints = false;
    auto pg = load_and_process_glyph(font, ctx, vstore, 2, opts);
    CHECK(pg.ok());
    if (!pg.ok()) return;
    CHECK(pg.value.points.size() == 12);
    if (pg.value.points.size() != 12) return;
    // The anchor is parent point 2 of the first component: (100, 100).
    CHECK(pg.value.points[4].x == 100 && pg.value.points[4].y == 100);
    CHECK(pg.value.points[8].x == 100 && pg.value.points[8].y == 100);
    CHECK(pg.value.points[10].x == 200 && pg.value.points[10].y == 200);
}


int main() {
    test_make_hint_context_cvt_cap();
    test_make_hint_context_storage_cap();
    test_point_matched_components_after_point_vector_grows();
    if (failures) {
        fprintf(stderr, "test_glyph_loader: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_glyph_loader: all passed\n");
    return 0;
}
