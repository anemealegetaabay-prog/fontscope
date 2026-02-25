#include "fontscope/bitmap.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

GlyphBitmap make_bitmap(uint16_t width, uint16_t height, uint8_t depth,
                        const uint8_t* data, size_t data_len)
{
    GlyphBitmap bm;
    bm.width  = width;
    bm.height = height;
    bm.depth  = depth;
    if (data && data_len)
        bm.data.assign(data, data + data_len);
    return bm;
}

// Extract one pixel from a packed row at the given x position.
// Returns the pixel value in the range [0, (1<<depth)-1].
static uint8_t extract_pixel(const uint8_t* row, uint16_t x, uint8_t depth) {
    switch (depth) {
    case 1: {
        uint8_t byte = row[x >> 3];
        return (byte >> (7 - (x & 7))) & 1;
    }
    case 2: {
        uint8_t byte = row[x >> 2];
        return (byte >> (6 - ((x & 3) << 1))) & 3;
    }
    case 4: {
        uint8_t byte = row[x >> 1];
        return (x & 1) ? (byte & 0xF) : (byte >> 4);
    }
    case 8:
        return row[x];
    default:
        return 0;
    }
}

void blend_bitmap_row(const GlyphBitmap& bm, uint16_t pixel_y,
                      uint8_t* dst, uint16_t dst_width,
                      const ColorEntry& color, uint8_t global_alpha)
{
    if (pixel_y >= bm.height || !dst || bm.data.empty()) return;
    if (bm.depth == 0) return;

    // stride computed as plain pixel width regardless of depth.
    // For depth < 8 the actual packed row is (width*depth+7)/8 bytes, which is
    // narrower. Using width as stride advances `row` too far into the buffer,
    // eventually reading past bm.data.end() for rows beyond the first.
    uint32_t row_stride = bm.width;
    const uint8_t* row = bm.data.data() + uint32_t(pixel_y) * row_stride;

    uint16_t copy_w = std::min(bm.width, dst_width);
    for (uint16_t x = 0; x < copy_w; ++x) {
        uint8_t pix = extract_pixel(row, x, bm.depth);
        uint8_t max_val = (bm.depth == 8) ? 255u : uint8_t((1u << bm.depth) - 1u);
        uint8_t alpha   = (max_val > 0)
                        ? uint8_t(uint32_t(pix) * global_alpha / max_val)
                        : 0u;

        // Alpha-blend color into destination byte (single channel for brevity).
        uint8_t src_a  = uint8_t(uint32_t(color.alpha) * alpha / 255u);
        uint8_t inv_a  = 255u - src_a;
        dst[x] = uint8_t((uint32_t(color.red) * src_a + uint32_t(dst[x]) * inv_a) / 255u);
    }
}

void render_bitmap_to_rgba(const GlyphBitmap& bm, const ColorEntry& color,
                            uint8_t* dst_rgba, uint16_t dst_w, uint16_t dst_h)
{
    if (!dst_rgba || bm.data.empty()) return;
    uint8_t depth = bm.depth ? bm.depth : 8;
    uint32_t packed_stride = (uint32_t(bm.width) * depth + 7) / 8;

    uint16_t h = std::min(bm.height, dst_h);
    uint16_t w = std::min(bm.width,  dst_w);

    for (uint16_t y = 0; y < h; ++y) {
        if (uint32_t(y) * packed_stride >= bm.data.size()) break;
        const uint8_t* row = bm.data.data() + uint32_t(y) * packed_stride;

        for (uint16_t x = 0; x < w; ++x) {
            uint8_t pix = extract_pixel(row, x, depth);
            uint8_t max_v = (depth == 8) ? 255u : uint8_t((1u << depth) - 1u);
            uint8_t a = max_v ? uint8_t(uint32_t(pix) * color.alpha / max_v) : 0u;

            uint8_t* px = dst_rgba + (uint32_t(y) * dst_w + x) * 4;
            px[0] = color.red;
            px[1] = color.green;
            px[2] = color.blue;
            px[3] = a;
        }
    }
}

} // namespace fontscope
