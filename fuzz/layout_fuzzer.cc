#include <cstdint>
#include <cstddef>
#include <vector>
#include "fontscope/inspect.h"
#include "fontscope/pipeline.h"
#include "fontscope/kern.h"

using namespace fontscope;

// Exercises the text-layout path: kern subtable parsing and pair lookup
// (formats 0 and 2) plus full string shaping with kerning applied.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;
    const FontFace& font = font_res.value;

    // Drive kern pair lookups directly over a bounded glyph-id range.
    const TableRecord* kern_rec = find_table(font.sfnt, tags::KERN());
    if (kern_rec) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(kern_rec->offset, kern_rec->length);
        auto kr = parse_kern(sub);
        if (kr.ok()) {
            uint16_t ng = font.maxp.num_glyphs;
            if (ng > 64) ng = 64;
            for (uint16_t a = 0; a < ng; ++a)
                for (uint16_t b = 0; b < ng; ++b)
                    (void)kr.value.lookup(a, b);
        }
    }

    // Shape a code-point run drawn from the tail of the input.
    std::vector<uint32_t> cps;
    size_t start = (size > 48) ? (size - 48) : 0;
    for (size_t i = start; i < size; ++i)
        cps.push_back(0x20u + (data[i] & 0x7Fu));

    if (!cps.empty()) {
        PipelineConfig cfg;
        cfg.ppem            = 16;
        cfg.antialiased     = false;
        cfg.apply_hints     = false;
        cfg.apply_variation = false;
        (void)render_string(font, cps, cfg);
    }

    return 0;
}
