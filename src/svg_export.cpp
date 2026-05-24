#include "fontscope/svg_export.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fontscope {

double svg_x(FUnit x, double scale, double tx) {
    return x * scale + tx;
}

double svg_y(FUnit y, double scale, double ty, bool flip, int vph) {
    double fy = y * scale + ty;
    return flip ? (double(vph) - fy) : fy;
}

std::string contour_to_svg_commands(
    const std::vector<FPoint>& pts,
    uint16_t start, uint16_t end, bool flip_y, double scale,
    double tx, double ty_off)
{
    if (start > end || end >= pts.size()) return {};

    char buf[256];
    std::string d;
    d.reserve((end - start + 1) * 24);

    auto sx = [&](FUnit x) { return svg_x(x, scale, tx); };
    auto sy = [&](FUnit y) {
        double fy = y * scale + ty_off;
        return flip_y ? -fy : fy;
    };

    // Find first on-curve point to start.
    int n = end - start + 1;
    int first_on = -1;
    for (int i = 0; i < n; ++i) {
        if (pts[start + i].on_curve) { first_on = i; break; }
    }

    int move_i = (first_on < 0) ? 0 : first_on;
    const FPoint& mp = pts[start + move_i];
    snprintf(buf, sizeof(buf), "M%.3f %.3f ", sx(mp.x), sy(mp.y));
    d += buf;

    int i = (move_i + 1) % n;
    while (i != move_i) {
        const FPoint& p = pts[start + i];
        int next = (i + 1) % n;
        const FPoint& q = pts[start + next];

        if (p.on_curve) {
            snprintf(buf, sizeof(buf), "L%.3f %.3f ", sx(p.x), sy(p.y));
            d += buf;
            i = next;
        } else {
            // Quadratic → cubic Bezier conversion.
            // If q is also off-curve, synthesize implied on-curve mid-point.
            if (!q.on_curve) {
                double mx = (sx(p.x) + sx(q.x)) * 0.5;
                double my = (sy(p.y) + sy(q.y)) * 0.5;
                // QC1 = CP1 * 2/3 + CP0 * 1/3
                const FPoint& prev_on = pts[start + ((i - 1 + n) % n)];
                double cx1 = sx(prev_on.x) + (sx(p.x) - sx(prev_on.x)) * 2.0/3.0;
                double cy1 = sy(prev_on.y) + (sy(p.y) - sy(prev_on.y)) * 2.0/3.0;
                double cx2 = mx + (sx(p.x) - mx) * 2.0/3.0;
                double cy2 = my + (sy(p.y) - my) * 2.0/3.0;
                snprintf(buf, sizeof(buf), "C%.3f %.3f %.3f %.3f %.3f %.3f ",
                         cx1, cy1, cx2, cy2, mx, my);
                d += buf;
                i = next;
            } else {
                const FPoint& prev_on = pts[start + ((i - 1 + n) % n)];
                double cx1 = sx(prev_on.x) + (sx(p.x) - sx(prev_on.x)) * 2.0/3.0;
                double cy1 = sy(prev_on.y) + (sy(p.y) - sy(prev_on.y)) * 2.0/3.0;
                double cx2 = sx(q.x)       + (sx(p.x) - sx(q.x))       * 2.0/3.0;
                double cy2 = sy(q.y)        + (sy(p.y) - sy(q.y))       * 2.0/3.0;
                snprintf(buf, sizeof(buf), "C%.3f %.3f %.3f %.3f %.3f %.3f ",
                         cx1, cy1, cx2, cy2, sx(q.x), sy(q.y));
                d += buf;
                i = (next + 1) % n;
            }
        }
    }
    d += "Z";
    return d;
}

std::string glyph_to_svg_path(const RawGlyph& g, const SvgExportOptions& opts) {
    if (g.is_empty() || g.is_composite()) return {};

    // Compute scale to fit glyph within viewport with padding.
    double ink_w = double(g.x_max - g.x_min);
    double ink_h = double(g.y_max - g.y_min);
    if (ink_w < 1) ink_w = 1;
    if (ink_h < 1) ink_h = 1;

    double pad = 0.05;
    double vw  = opts.viewport_width  * (1.0 - 2*pad);
    double vh  = opts.viewport_height * (1.0 - 2*pad);
    double scale = std::min(vw / ink_w, vh / ink_h);

    double tx = opts.viewport_width  * pad - g.x_min * scale;
    double ty;
    if (opts.flip_y) {
        ty = opts.viewport_height * (1.0 - pad) - g.y_min * scale;
        ty = -ty;  // applied as negation in sy()
    } else {
        ty = opts.viewport_height * pad - g.y_min * scale;
    }

    std::string d;
    uint16_t start = 0;
    for (uint16_t ep : g.end_pts_of_contours) {
        if (!d.empty()) d += " ";
        d += contour_to_svg_commands(g.points, start, ep,
                                     opts.flip_y, scale, tx, ty);
        start = ep + 1;
    }
    return d;
}

std::string glyph_to_svg(const FontFace& font, const RawGlyph& g,
                          uint16_t glyph_id, const SvgExportOptions& opts)
{
    (void)glyph_id;
    std::string path_d = glyph_to_svg_path(g, opts);

    char buf[1024];
    std::string svg;
    svg.reserve(1024 + path_d.size());

    snprintf(buf, sizeof(buf),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\""
        " width=\"%d\" height=\"%d\""
        " viewBox=\"0 0 %d %d\">\n",
        opts.viewport_width, opts.viewport_height,
        opts.viewport_width, opts.viewport_height);
    svg += buf;

    if (opts.add_metrics) {
        uint16_t upem = font.head.units_per_em ? font.head.units_per_em : 1000;
        double ink_w  = double(g.x_max - g.x_min);
        double scale  = opts.viewport_width / std::max(ink_w, double(upem));
        double asc_y  = opts.viewport_height - font.hhea.ascender  * scale;
        double dsc_y  = opts.viewport_height - font.hhea.descender * scale;
        snprintf(buf, sizeof(buf),
            "  <line x1=\"0\" y1=\"%.2f\" x2=\"%d\" y2=\"%.2f\""
            " stroke=\"#0088ff\" stroke-width=\"0.5\"/>\n"
            "  <line x1=\"0\" y1=\"%.2f\" x2=\"%d\" y2=\"%.2f\""
            " stroke=\"#ff4400\" stroke-width=\"0.5\"/>\n",
            asc_y, opts.viewport_width, asc_y,
            dsc_y, opts.viewport_width, dsc_y);
        svg += buf;
    }

    if (opts.add_bbox) {
        double ink_w = double(g.x_max - g.x_min);
        double ink_h = double(g.y_max - g.y_min);
        double scale = std::min(opts.viewport_width / std::max(ink_w, 1.0),
                                opts.viewport_height / std::max(ink_h, 1.0));
        double pad = 0.05;
        double bx = opts.viewport_width  * pad - g.x_min * scale;
        double by = opts.viewport_height * pad - g.y_min * scale;
        snprintf(buf, sizeof(buf),
            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\""
            " fill=\"none\" stroke=\"#aaaaaa\" stroke-width=\"0.5\""
            " stroke-dasharray=\"4,4\"/>\n",
            bx, by, ink_w * scale, ink_h * scale);
        svg += buf;
    }

    if (!path_d.empty()) {
        svg += "  <path fill=\"";
        svg += opts.fill_color;
        svg += "\"";
        if (!opts.stroke_color.empty() && opts.stroke_color != "none") {
            svg += " stroke=\"";
            svg += opts.stroke_color;
            snprintf(buf, sizeof(buf), "\" stroke-width=\"%.2f\"", opts.stroke_width);
            svg += buf;
        }
        snprintf(buf, sizeof(buf), " opacity=\"%.3f\" d=\"", opts.opacity);
        svg += buf;
        svg += path_d;
        svg += "\"/>\n";
    } else {
        svg += "  <!-- empty glyph -->\n";
    }

    svg += "</svg>\n";
    return svg;
}

std::string glyphs_to_svg_sheet(
    const FontFace&              font,
    const std::vector<uint16_t>& glyph_ids,
    uint16_t                     ppem,
    const SvgExportOptions&      opts)
{
    (void)ppem;
    int cell_w = opts.viewport_width;
    int cell_h = opts.viewport_height;
    int cols   = 8;
    int rows   = (int(glyph_ids.size()) + cols - 1) / cols;

    char buf[512];
    std::string svg;

    snprintf(buf, sizeof(buf),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\""
        " width=\"%d\" height=\"%d\""
        " viewBox=\"0 0 %d %d\">\n",
        cell_w * cols, cell_h * rows,
        cell_w * cols, cell_h * rows);
    svg += buf;

    SvgExportOptions cell_opts = opts;
    cell_opts.viewport_width  = cell_w;
    cell_opts.viewport_height = cell_h;

    for (size_t idx = 0; idx < glyph_ids.size(); ++idx) {
        uint16_t gid = glyph_ids[idx];
        int col = int(idx) % cols;
        int row = int(idx) / cols;

        auto rg = load_glyph(font, gid);
        if (!rg.ok()) continue;

        std::string path_d = glyph_to_svg_path(rg.value, cell_opts);

        int ox = col * cell_w;
        int oy = row * cell_h;

        snprintf(buf, sizeof(buf),
            "  <g transform=\"translate(%d,%d)\">\n", ox, oy);
        svg += buf;

        if (!path_d.empty()) {
            svg += "    <path fill=\"";
            svg += opts.fill_color;
            svg += "\" d=\"";
            svg += path_d;
            svg += "\"/>\n";
        }

        snprintf(buf, sizeof(buf),
            "    <text x=\"2\" y=\"%d\" font-size=\"10\" fill=\"#888\">%u</text>\n",
            cell_h - 2, gid);
        svg += buf;
        svg += "  </g>\n";
    }

    svg += "</svg>\n";
    return svg;
}

std::string font_to_svg(const FontFace& font, const SvgExportOptions& opts) {
    std::vector<uint16_t> ids;
    for (uint16_t g = 0; g < font.maxp.num_glyphs; ++g) ids.push_back(g);
    return glyphs_to_svg_sheet(font, ids, 24, opts);
}

} // namespace fontscope
