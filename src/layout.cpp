#include "fontscope/layout.h"
#include "fontscope/kern.h"
#include "fontscope/sfnt.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

std::vector<uint32_t> utf8_to_codepoints(const std::string& utf8) {
    std::vector<uint32_t> cps;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8.data());
    const uint8_t* end = p + utf8.size();

    while (p < end) {
        uint32_t cp = 0;
        uint8_t c = *p++;

        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
            if (p < end) cp = (cp << 6) | (*p++ & 0x3F);
        } else {
            cp = 0xFFFD;
        }

        cps.push_back(cp);
    }
    return cps;
}

std::string codepoints_to_utf8(const std::vector<uint32_t>& codepoints) {
    std::string out;
    out.reserve(codepoints.size());

    for (uint32_t cp : codepoints) {
        if (cp < 0x80) {
            out.push_back(char(cp));
        } else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

LineLayout shape_text_run(const FontFace& font, const TextRun& run) {
    LineLayout line{};

    uint16_t upem = font.head.units_per_em;
    if (upem == 0) upem = 1000;

    line.ascender  = scale_to_pixels(font.hhea.ascender,  run.ppem, upem);
    line.descender = scale_to_pixels(font.hhea.descender, run.ppem, upem);
    line.line_gap  = scale_to_pixels(font.hhea.line_gap,  run.ppem, upem);

    // Load kern table.
    KernTable kern;
    bool has_kern = false;
    const TableRecord* kern_rec = find_table(font.sfnt, tags::KERN());
    if (kern_rec) {
        ByteReader r(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = r.sub_reader(kern_rec->offset, kern_rec->length);
        auto kr = parse_kern(sub);
        if (kr.ok()) { kern = std::move(kr.value); has_kern = true; }
    }

    uint16_t prev_gid = 0;
    int32_t x_pen = 0;

    for (uint32_t i = 0; i < run.codepoints.size(); ++i) {
        uint32_t cp = run.codepoints[i];
        uint16_t gid = font.cmap.lookup(cp);

        GlyphItem item{};
        item.glyph_id = gid;
        item.cluster  = i;

        // Kerning.
        if (has_kern && prev_gid != 0 && gid != 0) {
            int16_t kern_val = kern.lookup(prev_gid, gid);
            item.x_offset = scale_to_pixels(kern_val, run.ppem, upem);
            x_pen += item.x_offset;
        }

        // Advance.
        GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, gid);
        item.x_advance = scale_to_pixels(hm.advance_width, run.ppem, upem);
        item.y_advance = 0;

        x_pen += item.x_advance;
        line.items.push_back(item);

        prev_gid = gid;
    }

    line.total_width = x_pen;
    return line;
}

int32_t measure_text_run(const FontFace& font, const TextRun& run) {
    return shape_text_run(font, run).total_width;
}

TextLayout layout_text(const FontFace& font, const TextRun& run, int32_t max_width) {
    TextLayout layout{};

    uint16_t upem = font.head.units_per_em ? font.head.units_per_em : 1000;
    layout.line_height = scale_to_pixels(font.hhea.ascender - font.hhea.descender
                                          + font.hhea.line_gap, run.ppem, upem);

    // Simple word-wrap: split on spaces.
    TextRun word_run = run;
    word_run.codepoints.clear();

    std::vector<std::vector<uint32_t>> words;
    std::vector<uint32_t> cur_word;

    for (uint32_t cp : run.codepoints) {
        if (cp == 0x20 || cp == 0x09 || cp == 0x0A) {
            if (!cur_word.empty()) words.push_back(cur_word);
            cur_word.clear();
            if (cp == 0x0A) {
                // Force line break.
                words.push_back({0x0A});
            }
        } else {
            cur_word.push_back(cp);
        }
    }
    if (!cur_word.empty()) words.push_back(cur_word);

    TextRun line_run = run;
    line_run.codepoints.clear();
    int32_t line_width = 0;

    auto flush_line = [&]() {
        if (!line_run.codepoints.empty()) {
            auto line = shape_text_run(font, line_run);
            layout.max_width = std::max(layout.max_width, line.total_width);
            layout.lines.push_back(std::move(line));
            line_run.codepoints.clear();
            line_width = 0;
        }
    };

    for (const auto& word : words) {
        if (word.size() == 1 && word[0] == 0x0A) {
            flush_line();
            continue;
        }

        // Measure the word.
        TextRun wr = run;
        wr.codepoints = word;
        int32_t ww = measure_text_run(font, wr);

        if (line_width > 0 && line_width + ww > max_width) {
            flush_line();
        }

        if (!line_run.codepoints.empty())
            line_run.codepoints.push_back(0x20);  // space between words

        line_run.codepoints.insert(line_run.codepoints.end(),
                                   word.begin(), word.end());
        line_width += ww;
    }
    flush_line();

    layout.total_height = int32_t(layout.lines.size()) * layout.line_height;
    return layout;
}

int32_t hit_test(const LineLayout& layout, int32_t pixel_x) {
    int32_t x = 0;
    for (size_t i = 0; i < layout.items.size(); ++i) {
        int32_t mid = x + layout.items[i].x_advance / 2;
        if (pixel_x < mid) return int32_t(i);
        x += layout.items[i].x_advance;
    }
    return int32_t(layout.items.size());
}

LineBBox line_bbox(const LineLayout& layout, const FontFace& font) {
    LineBBox bb{0, layout.descender, layout.total_width, layout.ascender};
    (void)font;
    return bb;
}

void reorder_bidi(std::vector<GlyphItem>& items, TextDirection dir) {
    if (dir == TextDirection::RTL)
        std::reverse(items.begin(), items.end());
}

std::vector<int32_t> cluster_advances(const LineLayout& layout) {
    std::vector<int32_t> advances;
    advances.reserve(layout.items.size());
    for (const auto& item : layout.items)
        advances.push_back(item.x_advance);
    return advances;
}

} // namespace fontscope
