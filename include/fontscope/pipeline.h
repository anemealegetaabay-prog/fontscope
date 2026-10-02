#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/errors.h"
#include "fontscope/inspect.h"
#include "fontscope/glyph_loader.h"
#include "fontscope/raster.h"
#include "fontscope/raster_aa.h"
#include "fontscope/variation.h"
#include "fontscope/colr.h"
#include "fontscope/cpal.h"
#include "fontscope/bitmap.h"

namespace fontscope {

// Configuration for the full render pipeline.
struct PipelineConfig {
    uint16_t ppem{16};               // pixels-per-em
    bool antialiased{true};
    bool apply_hints{true};
    bool apply_variation{true};
    std::vector<F2Dot14> normalized_coords;
    uint16_t palette_id{0};          // CPAL palette index for color glyphs
};

// Output of the render pipeline for one glyph.
struct RenderResult {
    RasterBuf  coverage;    // 8bpp grayscale coverage map
    int32_t    advance;     // scaled advance width in pixels
    int32_t    bearing;     // left-side bearing in pixels
};

// Main pipeline: load → variation → hint → IUP → rasterize.
Result<RenderResult> render_glyph_pipeline(const FontFace& font,
                                            uint16_t glyph_id,
                                            const PipelineConfig& cfg);

// Render a string of Unicode code points and return per-glyph results.
// Kerning is applied using the kern table if present.
struct StringRenderResult {
    std::vector<RenderResult> glyphs;
    std::vector<int32_t>      x_positions;  // pixel offset for each glyph
    int32_t total_width{0};
    int32_t ascender{0};
    int32_t descender{0};
};

StringRenderResult render_string(const FontFace& font,
                                  const std::vector<uint32_t>& codepoints,
                                  const PipelineConfig& cfg);

// Render a single glyph through the color layer pipeline (COLR/CPAL).
// Each layer is composited onto a shared ppem x ppem RGBA buffer (row pitch
// ppem * 4 bytes); parts of a layer outside it are clipped.
Result<std::vector<uint8_t>> render_color_glyph(const FontFace& font,
                                                  uint16_t glyph_id,
                                                  const PipelineConfig& cfg);

// Scale a FUnit value to pixels at the given ppem/upem.
inline int32_t scale_to_pixels(int32_t funits, uint16_t ppem, uint16_t upem) {
    if (upem == 0) return 0;
    return (funits * ppem + upem / 2) / upem;
}

// Convert a pixel count to 26.6 fixed-point.
inline int32_t pixels_to_26dot6(int32_t px) { return px << 6; }

} // namespace fontscope
