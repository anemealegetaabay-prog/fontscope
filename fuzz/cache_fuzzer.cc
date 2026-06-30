#include <cstdint>
#include <cstddef>
#include <vector>
#include "fontscope/inspect.h"
#include "fontscope/cache.h"
#include "fontscope/pipeline.h"

using namespace fontscope;

// Exercises the cached glyph-run renderer. A deliberately small cache budget
// forces eviction between adjacent glyphs, stressing the LRU lifecycle.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) return 0;

    auto font_res = load_font(data, size);
    if (!font_res.ok()) return 0;
    const FontFace& font = font_res.value;

    uint16_t n = font.maxp.num_glyphs;
    if (n > 32) n = 32;

    std::vector<uint16_t> gids;
    for (uint16_t g = 0; g < n; ++g) gids.push_back(g);
    if (gids.size() < 2) return 0;

    PipelineConfig cfg;
    cfg.ppem            = 16;
    cfg.antialiased     = false;
    cfg.apply_hints     = false;
    cfg.apply_variation = false;

    // Small cache budget typical of an embedded renderer; a run of several
    // glyphs spills it and exercises the LRU eviction path.
    GlyphCache cache(256);
    (void)render_run_cached(font, gids, cfg, cache);

    return 0;
}
