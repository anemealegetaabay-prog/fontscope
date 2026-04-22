#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/font_types.h"
#include "fontscope/fixed.h"

namespace fontscope {

// A 1-bit or 8-bit grayscale pixel buffer.
struct RasterBuf {
    uint32_t             width;
    uint32_t             height;
    uint32_t             stride;   // bytes per row
    uint8_t              depth;    // 1 or 8
    std::vector<uint8_t> pixels;

    void init(uint32_t w, uint32_t h, uint8_t d);
    void clear(uint8_t value = 0);

    void set_pixel(uint32_t x, uint32_t y, uint8_t v);
    uint8_t get_pixel(uint32_t x, uint32_t y) const;
};

// A scanline edge for the active-edge-list rasterizer.
struct Edge {
    int32_t  y_top;       // first scanline covered (26.6 fixed)
    int32_t  y_bot;       // last scanline covered  (26.6 fixed)
    Fixed16  x;           // current x intercept (16.16)
    Fixed16  dx;          // x step per scanline
    int8_t   winding;     // +1 or -1
};

// Build the edge list from a glyph outline (points + contour endpoints).
// Points are in design-space FUnits; scale converts to 26.6 pixel coords.
std::vector<Edge> build_edges(const std::vector<FPoint>& points,
                               const std::vector<uint16_t>& end_pts,
                               Fixed16 scale_x, Fixed16 scale_y,
                               Fixed16 offset_x, Fixed16 offset_y);

// Rasterize the edge list into a grayscale buffer using a non-zero winding rule.
void rasterize_nonzero(std::vector<Edge>& edges, RasterBuf& buf);

// Rasterize using even-odd fill rule.
void rasterize_evenodd(std::vector<Edge>& edges, RasterBuf& buf);

// Convert quadratic Bezier control points to lines for rasterization.
// Subdivides until the approximation error is below threshold.
void flatten_quadratic(const FPoint& p0, const FPoint& p1, const FPoint& p2,
                        std::vector<FPoint>& out, int depth = 0);

// Render a glyph outline to a grayscale buffer at the given ppem.
void render_glyph(const std::vector<FPoint>& points,
                  const std::vector<uint16_t>& end_pts,
                  uint16_t units_per_em, uint16_t ppem,
                  RasterBuf& buf);

} // namespace fontscope
