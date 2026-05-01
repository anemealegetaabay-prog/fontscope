#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/glyf.h"
#include "fontscope/hint_vm.h"
#include "fontscope/variation.h"
#include "fontscope/deltas.h"
#include "fontscope/inspect.h"

namespace fontscope {

// Options controlling which processing stages are applied.
struct LoadOptions {
    bool apply_hints{true};
    bool apply_variation{true};
    bool apply_iup{true};
    uint16_t ppem{16};
    std::vector<F2Dot14> normalized_coords;  // one per variation axis
};

// Fully processed glyph ready for rasterization.
struct ProcessedGlyph {
    std::vector<FPoint>   points;
    std::vector<uint16_t> end_pts;
    std::vector<uint8_t>  flags;
    FUnit  x_min, y_min, x_max, y_max;
    GlyphHMetrics hmetrics;
    bool is_composite{false};
    bool is_empty{true};
};

// Load, compose (for composites), apply variation deltas, run hints, and
// execute IUP for a single glyph.
Result<ProcessedGlyph> load_and_process_glyph(
    const FontFace& font,
    HintContext& hint_ctx,
    const VariationStore& vstore,
    uint16_t glyph_id,
    const LoadOptions& opts,
    int depth = 0);

// Convenience wrapper that creates a fresh HintContext.
Result<ProcessedGlyph> load_glyph_simple(const FontFace& font, uint16_t glyph_id,
                                          const LoadOptions& opts = {});

// Resolve a composite glyph by recursively loading and transforming components.
Result<ProcessedGlyph> resolve_composite(const FontFace& font,
                                          HintContext& hint_ctx,
                                          const VariationStore& vstore,
                                          const RawGlyph& composite,
                                          const LoadOptions& opts,
                                          int depth);

// Build a HintContext initialized from a FontFace's maxp and cvt tables.
HintContext make_hint_context(const FontFace& font, uint16_t glyph_id,
                               uint16_t ppem);

} // namespace fontscope
