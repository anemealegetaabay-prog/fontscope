#include "fontscope/glyph_loader.h"
#include "fontscope/sfnt.h"
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

int main() {
    test_make_hint_context_cvt_cap();
    test_make_hint_context_storage_cap();
    if (failures) {
        fprintf(stderr, "test_glyph_loader: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_glyph_loader: all passed\n");
    return 0;
}
