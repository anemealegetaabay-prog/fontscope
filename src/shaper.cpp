#include "fontscope/shaper.h"
#include "fontscope/tables.h"
#include "fontscope/sfnt.h"
#include "fontscope/reader.h"
#include "fontscope/pipeline.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

ShapeResult shape_simple(
    const FontFace&              font,
    const std::vector<uint32_t>& codepoints,
    const ShapeOptions&          opts)
{
    ShapeResult result{};
    result.rtl        = opts.rtl;
    result.script_tag = opts.script_tag;
    result.lang_tag   = opts.lang_tag;

    // If ppem and upem are available, compute scale. Otherwise use font units.
    uint16_t upem = font.head.units_per_em;
    if (upem == 0) upem = 1000;

    // Map codepoints to glyph IDs.
    result.glyphs.reserve(codepoints.size());
    for (size_t i = 0; i < codepoints.size(); ++i) {
        uint32_t cp  = codepoints[i];
        uint16_t gid = font.cmap.lookup(cp);

        ShapedGlyph sg{};
        sg.glyph_id = gid;
        sg.cluster  = uint32_t(i);

        GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, gid);
        int32_t aw = hm.advance_width;
        if (opts.ppem > 0) aw = scale_to_pixels(aw, opts.ppem, upem);
        sg.x_advance = aw;
        sg.y_advance = 0;
        sg.x_offset  = 0;
        sg.y_offset  = 0;

        result.glyphs.push_back(sg);
    }

    // Apply kerning.
    if (opts.apply_kern) {
        const TableRecord* kern_rec = find_table(font.sfnt, tags::KERN());
        if (kern_rec) {
            ByteReader r(font.raw_data.data(), font.raw_data.size());
            ByteReader sub = r.sub_reader(kern_rec->offset, kern_rec->length);
            auto kr = parse_kern(sub);
            if (kr.ok()) {
                KernTable kern = std::move(kr.value);
                apply_kern_to_glyphs(result.glyphs, kern);
                if (opts.ppem > 0) {
                    for (auto& g : result.glyphs) {
                        if (g.x_offset != 0)
                            g.x_offset = scale_to_pixels(g.x_offset, opts.ppem, upem);
                    }
                }
            }
        }
    }

    if (opts.rtl) reverse_glyphs(result.glyphs);

    return result;
}

void apply_kern_to_glyphs(
    std::vector<ShapedGlyph>& glyphs,
    const KernTable&           kern)
{
    for (size_t i = 0; i + 1 < glyphs.size(); ++i) {
        int16_t kv = kern.lookup(glyphs[i].glyph_id, glyphs[i+1].glyph_id);
        if (kv != 0) {
            glyphs[i].x_offset += kv;
            glyphs[i].x_advance += kv;
        }
    }
}

int32_t total_advance(const ShapeResult& result) {
    int32_t total = 0;
    for (const auto& g : result.glyphs) total += g.x_advance;
    return total;
}

std::vector<std::pair<size_t,size_t>>
cluster_spans(const std::vector<ShapedGlyph>& glyphs) {
    std::vector<std::pair<size_t,size_t>> spans;
    if (glyphs.empty()) return spans;

    size_t start = 0;
    uint32_t cur  = glyphs[0].cluster;
    for (size_t i = 1; i <= glyphs.size(); ++i) {
        uint32_t cl = (i < glyphs.size()) ? glyphs[i].cluster : cur + 1;
        if (cl != cur) {
            spans.emplace_back(start, i);
            start = i;
            cur   = cl;
        }
    }
    return spans;
}

void reverse_glyphs(std::vector<ShapedGlyph>& glyphs) {
    std::reverse(glyphs.begin(), glyphs.end());
}

uint16_t gsub_single_subst(
    uint16_t glyph_id,
    const std::unordered_map<uint16_t,uint16_t>& coverage,
    const std::vector<uint16_t>&                 subst)
{
    auto it = coverage.find(glyph_id);
    if (it == coverage.end()) return 0;
    uint16_t idx = it->second;
    if (idx >= subst.size()) return 0;
    return subst[idx];
}

LigatureMatch try_ligature(
    const std::vector<ShapedGlyph>& glyphs,
    size_t                          pos,
    const std::vector<LigatureSet>& sets)
{
    for (const auto& ls : sets) {
        if (ls.sequence.empty()) continue;
        if (pos + ls.sequence.size() >= glyphs.size()) continue;

        bool match = true;
        for (size_t i = 0; i < ls.sequence.size(); ++i) {
            if (glyphs[pos + 1 + i].glyph_id != ls.sequence[i]) {
                match = false;
                break;
            }
        }
        if (match) return {ls.output_glyph, ls.sequence.size() + 1};
    }
    return {0, 0};
}

// Approximate Unicode block → script tag mapping.
uint32_t codepoint_script(uint32_t cp) {
    if (cp < 0x0080) return script_tags::latn;  // Basic Latin
    if (cp < 0x0100) return script_tags::latn;  // Latin-1 Supplement
    if (cp < 0x0180) return script_tags::latn;  // Latin Extended-A/B
    if (cp >= 0x0400 && cp < 0x0500) return script_tags::cyrl;
    if (cp >= 0x0370 && cp < 0x0400) return script_tags::grek;
    if (cp >= 0x0600 && cp < 0x0700) return script_tags::arab;
    if (cp >= 0x0590 && cp < 0x0600) return script_tags::hebr;
    if (cp >= 0x0E00 && cp < 0x0E80) return script_tags::thai;
    if (cp >= 0x3040 && cp < 0x30A0) return script_tags::kana;  // Hiragana
    if (cp >= 0x30A0 && cp < 0x3100) return script_tags::kana;  // Katakana
    if (cp >= 0x4E00 && cp < 0xA000) return script_tags::hani;  // CJK
    if (cp >= 0x0900 && cp < 0x0980) return script_tags::deva;  // Devanagari
    return script_tags::DFLT;
}

bool text_likely_rtl(const std::vector<uint32_t>& codepoints) {
    int rtl_count = 0, ltr_count = 0;
    for (uint32_t cp : codepoints) {
        uint32_t sc = codepoint_script(cp);
        if (sc == script_tags::arab || sc == script_tags::hebr) ++rtl_count;
        else if (sc == script_tags::latn || sc == script_tags::cyrl ||
                 sc == script_tags::grek) ++ltr_count;
    }
    return rtl_count > ltr_count;
}

} // namespace fontscope
