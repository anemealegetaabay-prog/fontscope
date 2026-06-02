#include "fontscope/text_render.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace fontscope {

void RgbaBuffer::init(uint32_t w, uint32_t h, uint32_t fill) {
    width  = w;
    height = h;
    pixels.assign(size_t(w) * size_t(h), fill);
}

void RgbaBuffer::clear(uint32_t fill) {
    std::fill(pixels.begin(), pixels.end(), fill);
}

void RgbaBuffer::set_pixel(uint32_t x, uint32_t y, uint32_t rgba) {
    if (x >= width || y >= height) return;
    pixels[y * width + x] = rgba;
}

uint32_t RgbaBuffer::get_pixel(uint32_t x, uint32_t y) const {
    if (x >= width || y >= height) return 0;
    return pixels[y * width + x];
}

void RgbaBuffer::blend_alpha(uint32_t x, uint32_t y, uint8_t alpha, uint32_t color) {
    if (x >= width || y >= height) return;
    if (alpha == 0) return;

    uint32_t& dst = pixels[y * width + x];
    uint8_t cr = uint8_t((color >> 24) & 0xFF);
    uint8_t cg = uint8_t((color >> 16) & 0xFF);
    uint8_t cb = uint8_t((color >>  8) & 0xFF);

    uint8_t dr = uint8_t((dst >> 24) & 0xFF);
    uint8_t dg = uint8_t((dst >> 16) & 0xFF);
    uint8_t db = uint8_t((dst >>  8) & 0xFF);
    uint8_t da = uint8_t((dst      ) & 0xFF);

    uint32_t a = alpha;
    uint32_t ia = 255 - a;
    uint8_t nr = uint8_t((cr * a + dr * ia) / 255);
    uint8_t ng = uint8_t((cg * a + dg * ia) / 255);
    uint8_t nb = uint8_t((cb * a + db * ia) / 255);
    uint8_t na = uint8_t(da + (255 - da) * a / 255);
    dst = (uint32_t(nr) << 24) | (uint32_t(ng) << 16)
        | (uint32_t(nb) <<  8) |  uint32_t(na);
}

void RgbaBuffer::composite_coverage(const RasterBuf& cov, int32_t ox, int32_t oy,
                                     uint32_t color) {
    for (uint32_t row = 0; row < cov.height; ++row) {
        int32_t dst_y = oy + int32_t(row);
        if (dst_y < 0 || uint32_t(dst_y) >= height) continue;
        for (uint32_t col = 0; col < cov.width; ++col) {
            int32_t dst_x = ox + int32_t(col);
            if (dst_x < 0 || uint32_t(dst_x) >= width) continue;
            uint8_t alpha = cov.get_pixel(col, row);
            blend_alpha(uint32_t(dst_x), uint32_t(dst_y), alpha, color);
        }
    }
}

//

std::pair<int32_t,int32_t> measure_text(
    const FontFace&    font,
    const std::string& text,
    uint16_t           ppem)
{
    auto cps  = utf8_to_codepoints(text);
    TextRun run;
    run.codepoints = cps;
    run.ppem       = ppem;

    uint16_t upem = font.head.units_per_em ? font.head.units_per_em : 1000;
    auto line = shape_text_run(font, run);
    int32_t h = scale_to_pixels(font.hhea.ascender - font.hhea.descender
                                 + font.hhea.line_gap, ppem, upem);
    return {line.total_width, h};
}

RgbaBuffer render_text_to_rgba(
    const FontFace&           font,
    const std::string&        text,
    const TextRenderOptions&  opts)
{
    auto cps = utf8_to_codepoints(text);
    TextRun run;
    run.codepoints = cps;
    run.ppem       = opts.ppem;

    uint16_t upem = font.head.units_per_em ? font.head.units_per_em : 1000;

    TextLayout layout;
    if (opts.max_width > 0) {
        layout = layout_text(font, run, opts.max_width);
    } else {
        layout.lines.push_back(shape_text_run(font, run));
        if (!layout.lines.empty())
            layout.max_width = layout.lines[0].total_width;
        layout.line_height = scale_to_pixels(
            font.hhea.ascender - font.hhea.descender + font.hhea.line_gap,
            opts.ppem, upem);
        layout.total_height = layout.line_height;
    }

    int32_t canvas_w = layout.max_width + 2 * opts.padding_x;
    int32_t canvas_h = int32_t(layout.lines.size()) * layout.line_height
                     + 2 * opts.padding_y;
    if (canvas_w < 1) canvas_w = 1;
    if (canvas_h < 1) canvas_h = 1;

    RgbaBuffer buf;
    buf.init(uint32_t(canvas_w), uint32_t(canvas_h), opts.bg_color.to_rgba());

    PipelineConfig pcfg;
    pcfg.ppem        = opts.ppem;
    pcfg.antialiased = opts.antialiased;

    int32_t ascender = scale_to_pixels(font.hhea.ascender, opts.ppem, upem);
    uint32_t fg_color = opts.text_color.to_rgba();

    int32_t pen_y = opts.padding_y + ascender;

    for (const auto& line : layout.lines) {
        int32_t pen_x = opts.padding_x;

        for (const auto& item : line.items) {
            int32_t x_pos = pen_x + item.x_offset;

            auto rr = render_glyph_pipeline(font, item.glyph_id, pcfg);
            if (rr.ok()) {
                const RenderResult& r = rr.value;
                int32_t draw_x = x_pos + r.bearing;
                int32_t draw_y = pen_y - int32_t(r.coverage.height);
                buf.composite_coverage(r.coverage, draw_x, draw_y, fg_color);
            }
            pen_x += item.x_advance;
        }

        pen_y += layout.line_height;
    }

    return buf;
}

void render_text_at(
    RgbaBuffer&               buf,
    const FontFace&           font,
    const std::string&        text,
    int32_t x, int32_t y,
    const TextRenderOptions&  opts)
{
    RgbaBuffer tmp = render_text_to_rgba(font, text, opts);
    // Blit tmp onto buf at (x, y).
    for (uint32_t row = 0; row < tmp.height; ++row) {
        int32_t dy = y + int32_t(row);
        if (dy < 0 || uint32_t(dy) >= buf.height) continue;
        for (uint32_t col = 0; col < tmp.width; ++col) {
            int32_t dx = x + int32_t(col);
            if (dx < 0 || uint32_t(dx) >= buf.width) continue;
            uint32_t src = tmp.get_pixel(col, row);
            uint8_t  sa  = uint8_t(src & 0xFF);
            buf.blend_alpha(uint32_t(dx), uint32_t(dy), sa, src);
        }
    }
}

std::vector<uint8_t> rgba_to_ppm(const RgbaBuffer& buf) {
    // PPM P6: header then RGB pixels (no alpha).
    char header[64];
    int hlen = snprintf(header, sizeof(header),
                        "P6\n%u %u\n255\n", buf.width, buf.height);
    std::vector<uint8_t> out;
    out.reserve(size_t(hlen) + size_t(buf.width) * buf.height * 3);
    out.insert(out.end(), header, header + hlen);
    for (uint32_t px : buf.pixels) {
        out.push_back(uint8_t((px >> 24) & 0xFF));  // R
        out.push_back(uint8_t((px >> 16) & 0xFF));  // G
        out.push_back(uint8_t((px >>  8) & 0xFF));  // B
    }
    return out;
}

std::vector<uint8_t> rgba_alpha_to_pgm(const RgbaBuffer& buf) {
    // PGM P5: header then 8-bit alpha values.
    char header[64];
    int hlen = snprintf(header, sizeof(header),
                        "P5\n%u %u\n255\n", buf.width, buf.height);
    std::vector<uint8_t> out;
    out.reserve(size_t(hlen) + size_t(buf.width) * buf.height);
    out.insert(out.end(), header, header + hlen);
    for (uint32_t px : buf.pixels)
        out.push_back(uint8_t(px & 0xFF));
    return out;
}

} // namespace fontscope
