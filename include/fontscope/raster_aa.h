#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/raster.h"
#include "fontscope/font_types.h"

namespace fontscope {

// Coverage cell for supersampled anti-aliasing.
struct AaCell {
    int32_t cover;   // accumulated coverage from edge crossings
    int32_t area;    // partial area within the pixel
};

// Anti-aliased rasterizer using a 256-area coverage accumulator.
// Each pixel's final alpha = |area + cover * 256| / 512, clamped to [0,255].
struct AaRasterizer {
    uint32_t            width;
    uint32_t            height;
    std::vector<AaCell> cells;
    std::vector<uint8_t> scanline_buf;

    void init(uint32_t w, uint32_t h);
    void clear();

    // Render one edge segment from (x0,y0) to (x1,y1) in 26.6 pixel coords.
    void render_edge(int32_t x0, int32_t y0, int32_t x1, int32_t y1);

    // Finalize coverage into a grayscale buffer.
    void fill_nonzero(RasterBuf& buf);
    void fill_evenodd(RasterBuf& buf);

private:
    void render_scanline(int32_t y, int32_t x0, int32_t fy0,
                          int32_t x1, int32_t fy1);
    void set_cell(int32_t x, int32_t y, int32_t cover, int32_t area);
    AaCell& cell_at(int32_t x, int32_t y);
};

// Render a glyph with anti-aliasing.
void render_glyph_aa(const std::vector<FPoint>& points,
                     const std::vector<uint16_t>& end_pts,
                     uint16_t units_per_em, uint16_t ppem,
                     RasterBuf& buf);

// Convert the 8-bit grayscale raster to a 1-bit thresholded bitmap.
void threshold_to_1bpp(const RasterBuf& src, RasterBuf& dst, uint8_t threshold = 128);

// Apply a 3x3 Gaussian blur to the coverage buffer (softens edges slightly).
void blur_raster(RasterBuf& buf);

// Composite a foreground glyph buffer over an RGBA background.
void composite_over(const RasterBuf& glyph, uint32_t fg_color,
                    uint8_t* dst_rgba, uint32_t dst_w, uint32_t dst_h,
                    int32_t dx, int32_t dy);

} // namespace fontscope
