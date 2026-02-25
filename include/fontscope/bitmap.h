#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/cpal.h"

namespace fontscope {

// A sub-byte-packed or byte-per-pixel bitmap for a single glyph layer.
struct GlyphBitmap {
    uint16_t             width;
    uint16_t             height;
    uint8_t              depth;    // bits per pixel: 1, 2, 4, or 8
    std::vector<uint8_t> data;
};

// Blend one row of a sub-byte-packed bitmap into a linear 8bpp destination.
// dst is assumed to have at least dst_width bytes.
void blend_bitmap_row(const GlyphBitmap& bm, uint16_t pixel_y,
                      uint8_t* dst, uint16_t dst_width,
                      const ColorEntry& color, uint8_t global_alpha);

// Build a GlyphBitmap with packed pixel data from raw bytes.
GlyphBitmap make_bitmap(uint16_t width, uint16_t height, uint8_t depth,
                        const uint8_t* data, size_t data_len);

// Render the full bitmap into an RGBA destination buffer (width*height*4 bytes).
void render_bitmap_to_rgba(const GlyphBitmap& bm, const ColorEntry& color,
                            uint8_t* dst_rgba, uint16_t dst_w, uint16_t dst_h);

} // namespace fontscope
