#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/inspect.h"
#include "fontscope/pipeline.h"
#include "fontscope/layout.h"
#include "fontscope/raster.h"

namespace fontscope {

// A 32-bit RGBA pixel buffer.
struct RgbaBuffer {
    uint32_t              width;
    uint32_t              height;
    std::vector<uint32_t> pixels;  // RGBA packed, row-major

    void init(uint32_t w, uint32_t h, uint32_t fill = 0xFFFFFFFFu);
    void clear(uint32_t fill = 0xFFFFFFFFu);

    uint32_t* row(uint32_t y)       { return pixels.data() + y * width; }
    const uint32_t* row(uint32_t y) const { return pixels.data() + y * width; }

    void set_pixel(uint32_t x, uint32_t y, uint32_t rgba);
    uint32_t get_pixel(uint32_t x, uint32_t y) const;

    // Blend a single grayscale alpha value over the pixel at (x,y).
    // color is 0xRRGGBBAA.
    void blend_alpha(uint32_t x, uint32_t y, uint8_t alpha, uint32_t color);

    // Composite a grayscale coverage map at (ox,oy) with the given color.
    void composite_coverage(const RasterBuf& cov, int32_t ox, int32_t oy,
                             uint32_t color);
};

// Color specification (RGBA 8-bit components).
struct TextColor {
    uint8_t r, g, b, a;
    static TextColor black()  { return {0,0,0,255}; }
    static TextColor white()  { return {255,255,255,255}; }
    static TextColor transparent() { return {0,0,0,0}; }
    uint32_t to_rgba() const {
        return (uint32_t(r) << 24) | (uint32_t(g) << 16)
             | (uint32_t(b) << 8)  |  uint32_t(a);
    }
};

// Options for rendering text to an RgbaBuffer.
struct TextRenderOptions {
    uint16_t   ppem{16};
    TextColor  text_color   = TextColor::black();
    TextColor  bg_color     = TextColor::white();
    int32_t    max_width    = 0;    // 0 = no wrapping
    int32_t    padding_x    = 4;
    int32_t    padding_y    = 4;
    bool       antialiased  = true;
};

// Render a UTF-8 string to an RGBA pixel buffer.
// If max_width > 0, word-wraps within that pixel budget.
// Returns an RgbaBuffer sized to exactly fit the text (plus padding).
RgbaBuffer render_text_to_rgba(
    const FontFace&       font,
    const std::string&    text,
    const TextRenderOptions& opts = {});

// Render text into an existing RgbaBuffer at (x, y).
// Clips to the buffer bounds.
void render_text_at(
    RgbaBuffer&           buf,
    const FontFace&       font,
    const std::string&    text,
    int32_t x, int32_t y,
    const TextRenderOptions& opts = {});

// Compute the bounding box of a rendered string without actually drawing.
// Returns {width, height} in pixels.
std::pair<int32_t,int32_t> measure_text(
    const FontFace&       font,
    const std::string&    text,
    uint16_t              ppem);

// Encode an RgbaBuffer to a raw PPM file (P6 format) in memory.
// Useful for writing test output without dependencies.
std::vector<uint8_t> rgba_to_ppm(const RgbaBuffer& buf);

// Encode an RgbaBuffer to a grayscale PGM (P5) using alpha channel only.
std::vector<uint8_t> rgba_alpha_to_pgm(const RgbaBuffer& buf);

} // namespace fontscope
