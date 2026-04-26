#include "fontscope/raster_aa.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace fontscope {

void AaRasterizer::init(uint32_t w, uint32_t h) {
    width  = w;
    height = h;
    cells.assign(w * h, AaCell{0, 0});
    scanline_buf.assign(w, 0);
}

void AaRasterizer::clear() {
    std::fill(cells.begin(), cells.end(), AaCell{0, 0});
}

AaCell& AaRasterizer::cell_at(int32_t x, int32_t y) {
    static AaCell dummy{0, 0};
    if (x < 0 || y < 0 || uint32_t(x) >= width || uint32_t(y) >= height)
        return dummy;
    return cells[uint32_t(y) * width + uint32_t(x)];
}

void AaRasterizer::set_cell(int32_t x, int32_t y, int32_t cover, int32_t area) {
    AaCell& c = cell_at(x, y);
    c.cover += cover;
    c.area  += area;
}

// Render one horizontal span within a single pixel row.
// x0,x1 in 26.6; fy0,fy1 are fractional y within [0,64].
void AaRasterizer::render_scanline(int32_t y, int32_t x0, int32_t fy0,
                                    int32_t x1, int32_t fy1)
{
    int32_t cover = fy1 - fy0;
    if (cover == 0) return;

    int32_t fx0 = x0 & 63;
    int32_t fx1 = x1 & 63;
    int32_t cx0 = x0 >> 6;
    int32_t cx1 = x1 >> 6;

    if (cx0 == cx1) {
        // Single pixel: partial area.
        set_cell(cx0, y, cover, (fx0 + fx1) * cover);
        return;
    }

    // First partial pixel.
    int32_t first_area = (64 - fx0) * cover;
    set_cell(cx0, y, cover, first_area);

    // Full pixels in between.
    for (int32_t cx = cx0 + 1; cx < cx1; ++cx)
        set_cell(cx, y, cover, 64 * cover);

    // Last partial pixel.
    int32_t last_area = fx1 * cover;
    set_cell(cx1, y, cover, last_area);
}

void AaRasterizer::render_edge(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (y0 == y1) return;

    // Ensure top-to-bottom order.
    int32_t sign = 1;
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        sign = -1;
    }

    int32_t dy = y1 - y0;
    int32_t dx = x1 - x0;

    int32_t cy = y0 >> 6;
    int32_t cy_end = (y1 - 1) >> 6;

    int32_t fy0 = y0 & 63;
    int32_t x_cur = x0;

    while (cy <= cy_end) {
        int32_t fy1;
        int32_t x_next;

        if (cy < cy_end) {
            fy1 = 64;
            x_next = x0 + (int64_t(dx) * (64 * cy + 64 - y0)) / dy;
        } else {
            fy1 = y1 & 63;
            if (fy1 == 0) fy1 = 64;
            x_next = x1;
        }

        render_scanline(cy, std::min(x_cur, x_next),
                            fy0, std::max(x_cur, x_next), fy1);
        // Accumulate cover for full columns.
        int32_t col_cover = (fy1 - fy0) * sign;
        (void)col_cover;  // used via set_cell above

        fy0 = 0;
        x_cur = x_next;
        ++cy;
    }
}

void AaRasterizer::fill_nonzero(RasterBuf& buf) {
    buf.init(width, height, 8);
    buf.clear(0);

    for (uint32_t y = 0; y < height; ++y) {
        int32_t cover = 0;
        for (uint32_t x = 0; x < width; ++x) {
            AaCell& c = cells[y * width + x];
            int32_t alpha = std::abs(cover * 512 + c.area);
            alpha = std::min(alpha, 255 * 512) / 512;
            buf.set_pixel(x, y, uint8_t(alpha));
            cover += c.cover;
        }
    }
}

void AaRasterizer::fill_evenodd(RasterBuf& buf) {
    buf.init(width, height, 8);
    buf.clear(0);

    for (uint32_t y = 0; y < height; ++y) {
        int32_t cover = 0;
        for (uint32_t x = 0; x < width; ++x) {
            AaCell& c = cells[y * width + x];
            cover += c.cover;
            int32_t winding = std::abs(cover);
            int32_t alpha = (winding & ~1) ? 255 : 0;
            alpha = std::min(alpha, 255);
            buf.set_pixel(x, y, uint8_t(alpha));
        }
    }
}

void render_glyph_aa(const std::vector<FPoint>& points,
                     const std::vector<uint16_t>& end_pts,
                     uint16_t units_per_em, uint16_t ppem,
                     RasterBuf& buf)
{
    if (ppem == 0 || units_per_em == 0) return;

    double scale_val = double(ppem) / double(units_per_em);
    Fixed16 scale   = Fixed16::from_f64(scale_val);
    Fixed16 offset  = Fixed16::from_int(0);

    AaRasterizer aa;
    aa.init(ppem, ppem);

    // Convert outline to edges and feed to AA rasterizer.
    auto edges = build_edges(points, end_pts, scale, scale, offset,
                              Fixed16::from_int(ppem));
    for (const auto& e : edges) {
        // Convert from our Edge format back to 26.6 coordinates.
        int32_t x0 = fixed_to_26dot6(e.x);
        int32_t y0 = e.y_top * 64;
        int32_t x1 = fixed_to_26dot6(e.x + e.dx * (e.y_bot - e.y_top + 1));
        int32_t y1 = (e.y_bot + 1) * 64;
        aa.render_edge(x0, y0, x1, y1);
    }

    aa.fill_nonzero(buf);
}

void threshold_to_1bpp(const RasterBuf& src, RasterBuf& dst, uint8_t threshold) {
    dst.init(src.width, src.height, 1);
    for (uint32_t y = 0; y < src.height; ++y)
        for (uint32_t x = 0; x < src.width; ++x)
            dst.set_pixel(x, y, src.get_pixel(x,y) >= threshold ? 1 : 0);
}

void blur_raster(RasterBuf& buf) {
    if (buf.depth != 8) return;
    std::vector<uint8_t> tmp(buf.pixels);

    static const int8_t kernel[3][3] = {{1,2,1},{2,4,2},{1,2,1}};
    const int kw = 16;  // sum of kernel

    for (uint32_t y = 1; y + 1 < buf.height; ++y) {
        for (uint32_t x = 1; x + 1 < buf.width; ++x) {
            int32_t sum = 0;
            for (int ky = -1; ky <= 1; ++ky)
                for (int kx = -1; kx <= 1; ++kx)
                    sum += int32_t(tmp[(y+ky)*buf.stride+(x+kx)]) * kernel[ky+1][kx+1];
            buf.pixels[y * buf.stride + x] = uint8_t(sum / kw);
        }
    }
}

void composite_over(const RasterBuf& glyph, uint32_t fg_color,
                    uint8_t* dst_rgba, uint32_t dst_w, uint32_t dst_h,
                    int32_t dx, int32_t dy)
{
    if (!dst_rgba) return;

    uint8_t fr = (fg_color >> 16) & 0xFF;
    uint8_t fg = (fg_color >>  8) & 0xFF;
    uint8_t fb = (fg_color      ) & 0xFF;
    uint8_t fa = (fg_color >> 24) & 0xFF;

    for (uint32_t gy = 0; gy < glyph.height; ++gy) {
        int32_t py = int32_t(gy) + dy;
        if (py < 0 || uint32_t(py) >= dst_h) continue;

        for (uint32_t gx = 0; gx < glyph.width; ++gx) {
            int32_t px = int32_t(gx) + dx;
            if (px < 0 || uint32_t(px) >= dst_w) continue;

            uint8_t cov = glyph.get_pixel(gx, gy);
            uint8_t src_a = uint8_t(uint32_t(fa) * cov / 255u);
            uint8_t inv_a = 255 - src_a;

            uint8_t* dst = dst_rgba + (uint32_t(py) * dst_w + uint32_t(px)) * 4;
            dst[0] = uint8_t((uint32_t(fr) * src_a + uint32_t(dst[0]) * inv_a) / 255u);
            dst[1] = uint8_t((uint32_t(fg) * src_a + uint32_t(dst[1]) * inv_a) / 255u);
            dst[2] = uint8_t((uint32_t(fb) * src_a + uint32_t(dst[2]) * inv_a) / 255u);
            dst[3] = uint8_t(src_a + uint32_t(dst[3]) * inv_a / 255u);
        }
    }
}

} // namespace fontscope
