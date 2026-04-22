#include "fontscope/raster.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace fontscope {

void RasterBuf::init(uint32_t w, uint32_t h, uint8_t d) {
    width  = w;
    height = h;
    depth  = d;
    stride = (d == 1) ? ((w + 7) / 8) : w;
    pixels.assign(stride * h, 0);
}

void RasterBuf::clear(uint8_t value) {
    std::fill(pixels.begin(), pixels.end(), value);
}

void RasterBuf::set_pixel(uint32_t x, uint32_t y, uint8_t v) {
    if (x >= width || y >= height) return;
    if (depth == 8) {
        pixels[y * stride + x] = v;
    } else {
        uint32_t byte = y * stride + (x >> 3);
        uint8_t  bit  = 7 - (x & 7);
        if (v) pixels[byte] |=  (1u << bit);
        else   pixels[byte] &= ~(1u << bit);
    }
}

uint8_t RasterBuf::get_pixel(uint32_t x, uint32_t y) const {
    if (x >= width || y >= height) return 0;
    if (depth == 8) return pixels[y * stride + x];
    uint32_t byte = y * stride + (x >> 3);
    uint8_t  bit  = 7 - (x & 7);
    return (pixels[byte] >> bit) & 1;
}

// Convert design-space FPoint to 26.6 pixel coordinates.
static int32_t to_26dot6(FUnit v, Fixed16 scale, Fixed16 offset) {
    Fixed16 pix = Fixed16::from_int(v) * scale + offset;
    return fixed_to_26dot6(pix);
}

// Check if a contour goes clockwise (positive winding) in screen coords.
static int8_t contour_winding(const std::vector<FPoint>& pts,
                               uint16_t start, uint16_t end,
                               Fixed16 scale_x, Fixed16 scale_y)
{
    // Shoelace formula sign.
    int64_t sum = 0;
    for (uint16_t i = start; i <= end; ++i) {
        uint16_t j = (i == end) ? start : i + 1;
        int64_t x1 = int64_t(pts[i].x) * scale_x.raw;
        int64_t y1 = int64_t(pts[i].y) * scale_y.raw;
        int64_t x2 = int64_t(pts[j].x) * scale_x.raw;
        int64_t y2 = int64_t(pts[j].y) * scale_y.raw;
        sum += (x2 - x1) * (y2 + y1);
    }
    return sum >= 0 ? 1 : -1;
}

void flatten_quadratic(const FPoint& p0, const FPoint& p1, const FPoint& p2,
                        std::vector<FPoint>& out, int depth)
{
    if (depth > 10) { out.push_back(p2); return; }

    // Midpoints.
    FPoint m01 = {FUnit((int32_t(p0.x)+p1.x)/2), FUnit((int32_t(p0.y)+p1.y)/2), false};
    FPoint m12 = {FUnit((int32_t(p1.x)+p2.x)/2), FUnit((int32_t(p1.y)+p2.y)/2), false};
    FPoint mid = {FUnit((int32_t(m01.x)+m12.x)/2), FUnit((int32_t(m01.y)+m12.y)/2), true};

    // Flatness criterion: if the control point is close enough to the midpoint.
    int32_t dx = int32_t(mid.x) - int32_t(p1.x);
    int32_t dy = int32_t(mid.y) - int32_t(p1.y);
    if (dx*dx + dy*dy <= 1) {
        out.push_back(p2);
        return;
    }

    flatten_quadratic(p0, m01, mid, out, depth+1);
    flatten_quadratic(mid, m12, p2, out, depth+1);
}

// Flatten contour into a list of on-curve points (resolving off-curve runs).
static std::vector<FPoint> flatten_contour(const std::vector<FPoint>& pts,
                                            uint16_t start, uint16_t end)
{
    std::vector<FPoint> result;
    uint16_t n = end - start + 1;
    if (n == 0) return result;

    // Find first on-curve point.
    uint16_t first_on = start;
    while (first_on <= end && !pts[first_on].on_curve) ++first_on;
    if (first_on > end) {
        // All off-curve: synthesize midpoints.
        for (uint16_t i = 0; i < n; ++i) {
            uint16_t j = start + i;
            uint16_t k = start + (i+1) % n;
            FPoint synth = {FUnit((int32_t(pts[j].x)+pts[k].x)/2),
                             FUnit((int32_t(pts[j].y)+pts[k].y)/2), true};
            flatten_quadratic(synth, pts[j], {FUnit((int32_t(pts[j].x)+pts[k].x)/2),
                                              FUnit((int32_t(pts[j].y)+pts[k].y)/2), true},
                               result);
        }
        return result;
    }

    result.push_back(pts[first_on]);

    uint16_t i = first_on + 1;
    while (i != first_on) {
        if (i > end) i = start;
        if (i == first_on) break;

        const FPoint& cur = pts[i];
        if (cur.on_curve) {
            result.push_back(cur);
        } else {
            // Off-curve: look ahead for the next point.
            uint16_t next = (i < end) ? i+1 : start;
            const FPoint& p2 = pts[next];
            FPoint p0 = result.back();
            FPoint ctrl = cur;

            if (!p2.on_curve) {
                // Two consecutive off-curve: synthesize midpoint as on-curve.
                FPoint implied = {FUnit((int32_t(cur.x)+p2.x)/2),
                                   FUnit((int32_t(cur.y)+p2.y)/2), true};
                flatten_quadratic(p0, ctrl, implied, result);
            } else {
                flatten_quadratic(p0, ctrl, p2, result);
                ++i;  // skip the on-curve we already consumed
                if (i > end) i = start;
                if (i != first_on) result.push_back(pts[i]);
            }
        }
        ++i;
        if (i > end) i = start;
    }

    return result;
}

std::vector<Edge> build_edges(const std::vector<FPoint>& points,
                               const std::vector<uint16_t>& end_pts,
                               Fixed16 scale_x, Fixed16 scale_y,
                               Fixed16 offset_x, Fixed16 offset_y)
{
    std::vector<Edge> edges;
    uint16_t contour_start = 0;

    for (uint16_t c = 0; c < uint16_t(end_pts.size()); ++c) {
        uint16_t contour_end = end_pts[c];
        if (contour_end < contour_start || contour_end >= points.size()) {
            contour_start = contour_end + 1;
            continue;
        }

        std::vector<FPoint> flat = flatten_contour(points, contour_start, contour_end);
        int8_t wind = contour_winding(points, contour_start, contour_end,
                                       scale_x, scale_y);

        size_t n = flat.size();
        for (size_t i = 0; i < n; ++i) {
            const FPoint& a = flat[i];
            const FPoint& b = flat[(i+1) % n];

            int32_t x0 = to_26dot6(a.x, scale_x, offset_x);
            int32_t y0 = to_26dot6(a.y, scale_y, offset_y);
            int32_t x1 = to_26dot6(b.x, scale_x, offset_x);
            int32_t y1 = to_26dot6(b.y, scale_y, offset_y);

            if (y0 == y1) continue;  // horizontal edge

            // Ensure top-to-bottom ordering.
            int8_t w = wind;
            if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); w = -w; }

            int32_t dy = y1 - y0;
            Edge e{};
            e.y_top    = y0 >> 6;
            e.y_bot    = (y1 - 1) >> 6;
            e.x        = Fixed16::from_raw((x0 << 10));  // 26.6 → 16.16
            e.dx       = Fixed16::from_raw(((x1 - x0) << 10) / (dy ? dy : 1));
            e.winding  = w;
            edges.push_back(e);
        }

        contour_start = contour_end + 1;
    }
    return edges;
}

void rasterize_nonzero(std::vector<Edge>& edges, RasterBuf& buf) {
    if (edges.empty() || buf.pixels.empty()) return;

    // Sort edges by y_top then x.
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return a.y_top < b.y_top || (a.y_top == b.y_top && a.x < b.x);
    });

    std::vector<Edge*> active;

    for (int32_t y = 0; y < int32_t(buf.height); ++y) {
        // Activate edges that start on this scanline.
        for (auto& e : edges)
            if (e.y_top == y) active.push_back(&e);

        // Remove edges that ended above this scanline.
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [y](const Edge* e){ return e->y_bot < y; }),
                     active.end());

        // Sort active edges by current x.
        std::sort(active.begin(), active.end(),
                  [](const Edge* a, const Edge* b){ return a->x < b->x; });

        // Fill using non-zero winding rule.
        int winding = 0;
        for (size_t i = 0; i + 1 < active.size(); ++i) {
            winding += active[i]->winding;
            if (winding != 0) {
                int32_t x0 = std::max(0, active[i]->x.integer());
                int32_t x1 = std::min(int32_t(buf.width)-1, active[i+1]->x.integer());
                for (int32_t x = x0; x <= x1; ++x)
                    buf.set_pixel(uint32_t(x), uint32_t(y), 255);
            }
        }

        // Advance active edge x positions.
        for (auto* e : active)
            e->x = e->x + e->dx;
    }
}

void rasterize_evenodd(std::vector<Edge>& edges, RasterBuf& buf) {
    if (edges.empty() || buf.pixels.empty()) return;

    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return a.y_top < b.y_top;
    });

    std::vector<Edge*> active;

    for (int32_t y = 0; y < int32_t(buf.height); ++y) {
        for (auto& e : edges)
            if (e.y_top == y) active.push_back(&e);

        active.erase(std::remove_if(active.begin(), active.end(),
                                    [y](const Edge* e){ return e->y_bot < y; }),
                     active.end());

        std::sort(active.begin(), active.end(),
                  [](const Edge* a, const Edge* b){ return a->x < b->x; });

        for (size_t i = 0; i + 1 < active.size(); i += 2) {
            int32_t x0 = std::max(0, active[i]->x.integer());
            int32_t x1 = std::min(int32_t(buf.width)-1, active[i+1]->x.integer());
            for (int32_t x = x0; x <= x1; ++x)
                buf.set_pixel(uint32_t(x), uint32_t(y), 255);
        }

        for (auto* e : active)
            e->x = e->x + e->dx;
    }
}

void render_glyph(const std::vector<FPoint>& points,
                  const std::vector<uint16_t>& end_pts,
                  uint16_t units_per_em, uint16_t ppem,
                  RasterBuf& buf)
{
    if (ppem == 0 || units_per_em == 0) return;

    double scale_val = double(ppem) / double(units_per_em);
    Fixed16 scale = Fixed16::from_f64(scale_val);

    buf.init(ppem, ppem, 8);
    buf.clear(0);

    auto edges = build_edges(points, end_pts, scale, scale,
                              Fixed16::from_int(0),
                              Fixed16::from_int(ppem));
    rasterize_nonzero(edges, buf);
}

} // namespace fontscope
