#include <cstdint>
#include <cstddef>
#include <algorithm>
#include "fontscope/inspect.h"
#include "fontscope/glyph_loader.h"
#include "fontscope/hint_vm.h"

using namespace fontscope;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;

    const FontFace& font = font_res.value;
    uint16_t n = font.maxp.num_glyphs;
    if (n > 64) n = 64;

    LoadOptions opts;
    opts.ppem            = 16;
    opts.apply_hints     = true;
    opts.apply_variation = false;
    opts.apply_iup       = true;

    VariationStore empty;
    for (uint16_t gid = 0; gid < n; ++gid) {
        HintContext ctx = make_hint_context(font, gid, opts.ppem);
        (void)load_and_process_glyph(font, ctx, empty, gid, opts);
    }

    return 0;
}
