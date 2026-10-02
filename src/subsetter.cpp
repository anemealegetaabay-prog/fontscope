#include "fontscope/subsetter.h"
#include "fontscope/sfnt.h"
#include "fontscope/reader.h"
#include "fontscope/loca.h"
#include "fontscope/glyf.h"
#include "fontscope/cmap.h"
#include <cstring>
#include <algorithm>
#include <cassert>

namespace fontscope {

void emit_u16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(uint8_t(v >> 8));
    out.push_back(uint8_t(v & 0xFF));
}

void emit_u32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(uint8_t(v >> 24));
    out.push_back(uint8_t((v >> 16) & 0xFF));
    out.push_back(uint8_t((v >> 8)  & 0xFF));
    out.push_back(uint8_t(v & 0xFF));
}

void patch_u32(std::vector<uint8_t>& out, size_t offset, uint32_t v) {
    if (offset + 4 > out.size()) return;
    out[offset + 0] = uint8_t(v >> 24);
    out[offset + 1] = uint8_t((v >> 16) & 0xFF);
    out[offset + 2] = uint8_t((v >> 8)  & 0xFF);
    out[offset + 3] = uint8_t(v & 0xFF);
}

uint32_t sfnt_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    size_t words = (len + 3) / 4;
    for (size_t i = 0; i < words; ++i) {
        uint32_t w = 0;
        for (int b = 0; b < 4; ++b) {
            w <<= 8;
            size_t idx = i * 4 + b;
            if (idx < len) w |= data[idx];
        }
        sum += w;
    }
    return sum;
}

void pad_to_4(std::vector<uint8_t>& out) {
    while (out.size() % 4 != 0) out.push_back(0);
}

void collect_component_gids(
    const FontFace&                font,
    uint16_t                       glyph_id,
    std::unordered_set<uint16_t>&  visited)
{
    if (visited.count(glyph_id)) return;
    visited.insert(glyph_id);

    auto res = load_glyph(font, glyph_id);
    if (!res.ok()) return;

    if (res.value.is_composite()) {
        for (const auto& comp : res.value.components)
            collect_component_gids(font, comp.glyph_index, visited);
    }
}

std::vector<uint16_t> build_glyph_order(const std::unordered_set<uint16_t>& gids) {
    std::vector<uint16_t> order(gids.begin(), gids.end());
    std::sort(order.begin(), order.end());
    return order;
}

std::unordered_map<uint16_t,uint16_t> build_gid_map(const std::vector<uint16_t>& order) {
    std::unordered_map<uint16_t,uint16_t> m;
    for (size_t i = 0; i < order.size(); ++i)
        m[order[i]] = uint16_t(i);
    return m;
}

// Build a format-4 cmap covering a mapping of old-GID → new-GID for the given codepoints.
static std::vector<uint8_t> build_cmap4(
    const FontFace&                               font,
    const std::unordered_map<uint16_t,uint16_t>&  gid_map,
    const std::vector<uint32_t>&                  codepoints)
{
    std::vector<uint8_t> out;
    // cmap header: version=0, numTables=1
    emit_u16(out, 0);   // version
    emit_u16(out, 1);   // numTables
    // Encoding record: platformId=3, encodingId=1, offset=12
    emit_u16(out, 3);
    emit_u16(out, 1);
    emit_u32(out, 12);  // offset to subtable

    // Collect BMP codepoint→new-GID pairs.
    std::vector<std::pair<uint16_t,uint16_t>> pairs;
    for (uint32_t cp : codepoints) {
        if (cp > 0xFFFF) continue;
        uint16_t old_gid = font.cmap.lookup(cp);
        auto it = gid_map.find(old_gid);
        if (it == gid_map.end()) continue;
        pairs.emplace_back(uint16_t(cp), it->second);
    }
    std::sort(pairs.begin(), pairs.end());

    if (pairs.empty()) {
        // Emit empty format-4 subtable.
        emit_u16(out, 4);   // format
        emit_u16(out, 32);  // length (minimum)
        emit_u16(out, 0);   // language
        emit_u16(out, 0);   // segCountX2
        emit_u16(out, 0); emit_u16(out, 0); emit_u16(out, 0);
        // terminator segment
        emit_u16(out, 0xFFFF); // endCode
        emit_u16(out, 0);      // reservedPad
        emit_u16(out, 0xFFFF); // startCode
        emit_u16(out, 1);      // idDelta
        emit_u16(out, 0);      // idRangeOffset
        return out;
    }

    // Build simple contiguous segments.
    struct Seg { uint16_t start, end, delta; };
    std::vector<Seg> segs;
    Seg cur{pairs[0].first, pairs[0].first,
            uint16_t(int(pairs[0].second) - int(pairs[0].first))};
    for (size_t i = 1; i < pairs.size(); ++i) {
        uint16_t cp  = pairs[i].first;
        uint16_t gid = pairs[i].second;
        uint16_t exp_delta = uint16_t(int(gid) - int(cp));
        if (cp == cur.end + 1 && exp_delta == cur.delta) {
            cur.end = cp;
        } else {
            segs.push_back(cur);
            cur = {cp, cp, exp_delta};
        }
    }
    segs.push_back(cur);
    // Terminator.
    segs.push_back({0xFFFF, 0xFFFF, 1});

    uint16_t seg_count = uint16_t(segs.size());
    uint16_t search_range = 1;
    int entry_selector    = 0;
    while (search_range * 2 <= seg_count) { search_range *= 2; ++entry_selector; }
    uint16_t range_shift  = uint16_t(seg_count - search_range);

    // Format-4 subtable (no glyphIdArray, all delta-based).
    uint16_t subtable_len = uint16_t(14 + seg_count * 8);
    emit_u16(out, 4);
    emit_u16(out, subtable_len);
    emit_u16(out, 0);  // language
    emit_u16(out, uint16_t(seg_count * 2));
    emit_u16(out, uint16_t(search_range * 2));
    emit_u16(out, uint16_t(entry_selector));
    emit_u16(out, uint16_t(range_shift * 2));
    for (const auto& s : segs) emit_u16(out, s.end);
    emit_u16(out, 0);  // reservedPad
    for (const auto& s : segs) emit_u16(out, s.start);
    for (const auto& s : segs) emit_u16(out, s.delta);
    for (size_t i = 0; i < segs.size(); ++i) emit_u16(out, 0);  // idRangeOffset=0

    return out;
}

// Extract raw table bytes from the source font.
static std::vector<uint8_t> extract_table(
    const FontFace& font, Tag tag)
{
    const TableRecord* rec = find_table(font.sfnt, tag);
    if (!rec || rec->offset + rec->length > font.raw_data.size()) return {};
    return std::vector<uint8_t>(
        font.raw_data.begin() + rec->offset,
        font.raw_data.begin() + rec->offset + rec->length);
}

// Build a new loca + glyf pair from the subset glyph order.
static void build_glyf_loca(
    const FontFace&                              font,
    const std::vector<uint16_t>&                 order,
    const std::unordered_map<uint16_t,uint16_t>& gid_map,
    bool                                         drop_hints,
    std::vector<uint8_t>&                        glyf_out,
    std::vector<uint8_t>&                        loca_out,
    bool&                                        use_long_loca)
{
    // Collect raw glyph data for each glyph in order.
    const TableRecord* glyf_rec = find_table(font.sfnt, tags::GLYF());
    if (!glyf_rec) return;

    std::vector<std::vector<uint8_t>> glyph_data(order.size());

    for (size_t i = 0; i < order.size(); ++i) {
        uint16_t gid = order[i];
        GlyfRange range = font.loca.glyph_range(gid);
        if (range.begin == range.end) {
            glyph_data[i] = {};  // empty glyph
            continue;
        }
        uint32_t off = glyf_rec->offset + range.begin;
        uint32_t len = range.end - range.begin;
        if (off + len > font.raw_data.size()) { glyph_data[i] = {}; continue; }

        glyph_data[i].assign(
            font.raw_data.begin() + off,
            font.raw_data.begin() + off + len);

        // If this is a composite glyph, remap component GIDs.
        auto& gd = glyph_data[i];
        if (gd.size() >= 10) {
            int16_t n_contours = int16_t((gd[0] << 8) | gd[1]);
            if (n_contours < 0) {
                // Walk composite components at offset 10.
                size_t pos = 10;
                while (pos + 4 <= gd.size()) {
                    uint16_t flags      = uint16_t((gd[pos] << 8) | gd[pos+1]);
                    uint16_t comp_gid   = uint16_t((gd[pos+2] << 8) | gd[pos+3]);
                    auto it = gid_map.find(comp_gid);
                    if (it != gid_map.end()) {
                        gd[pos+2] = uint8_t(it->second >> 8);
                        gd[pos+3] = uint8_t(it->second & 0xFF);
                    }
                    // Advance past this component.
                    pos += 4;
                    if (flags & 0x0001) pos += 4; else pos += 2;  // args
                    if (flags & 0x0008) pos += 2;  // WE_HAVE_A_SCALE
                    else if (flags & 0x0040) pos += 4;  // WE_HAVE_AN_X_AND_Y_SCALE
                    else if (flags & 0x0080) pos += 8;  // WE_HAVE_A_2X2
                    if (!(flags & 0x0020)) break;  // MORE_COMPONENTS
                }
            } else if (drop_hints && size_t(n_contours) > 0) {
                // Clear instruction length field if present.
                // Simple glyph instruction count is at endPts[n_contours]+2.
                size_t inst_offset = 10 + size_t(n_contours) * 2;
                if (inst_offset + 2 <= gd.size()) {
                    uint16_t inst_len = uint16_t((gd[inst_offset] << 8) | gd[inst_offset+1]);
                    if (inst_len > 0 && inst_offset + 2 + inst_len <= gd.size()) {
                        gd[inst_offset]   = 0;
                        gd[inst_offset+1] = 0;
                        gd.erase(gd.begin() + inst_offset + 2,
                                 gd.begin() + inst_offset + 2 + inst_len);
                    }
                }
            }
        }
    }

    // Build glyf table.
    glyf_out.clear();
    std::vector<uint32_t> offsets;
    for (const auto& gd : glyph_data) {
        offsets.push_back(uint32_t(glyf_out.size()));
        glyf_out.insert(glyf_out.end(), gd.begin(), gd.end());
        while (glyf_out.size() % 4 != 0) glyf_out.push_back(0);
    }
    offsets.push_back(uint32_t(glyf_out.size()));

    // Decide loca format.
    use_long_loca = (glyf_out.size() > 0xFFFE * 2);
    loca_out.clear();
    if (use_long_loca) {
        for (uint32_t off : offsets) emit_u32(loca_out, off);
    } else {
        for (uint32_t off : offsets) emit_u16(loca_out, uint16_t(off / 2));
    }
}

SubsetResult subset_font_by_gids(
    const FontFace&                       font,
    const std::unordered_set<uint16_t>&   glyph_ids,
    const SubsetOptions&                  opts)
{
    SubsetResult res{};

    // Always include glyph 0 (notdef).
    std::unordered_set<uint16_t> full_set = glyph_ids;
    full_set.insert(0);

    // Transitively add components.
    std::unordered_set<uint16_t> expanded;
    for (uint16_t g : full_set) collect_component_gids(font, g, expanded);

    auto order  = build_glyph_order(expanded);
    auto gid_map = build_gid_map(order);
    res.gid_map   = gid_map;
    res.num_glyphs = uint16_t(order.size());

    // Build new glyf + loca.
    std::vector<uint8_t> new_glyf, new_loca;
    bool long_loca = false;
    build_glyf_loca(font, order, gid_map, !opts.retain_hints,
                    new_glyf, new_loca, long_loca);

    // Tables to carry over verbatim (in order).
    struct TableEntry {
        uint32_t tag;
        std::vector<uint8_t> data;
    };
    std::vector<TableEntry> tables;

    auto add_raw = [&](Tag tag) {
        auto d = extract_table(font, tag);
        if (!d.empty()) tables.push_back({tag.value, std::move(d)});
    };

    // head: patch indexToLocFormat.
    {
        auto d = extract_table(font, tags::HEAD());
        if (d.size() >= 50) {
            d[50] = 0;
            d[51] = long_loca ? 1 : 0;
            // Zero checksumAdjustment for now (will patch after full assembly).
            d[8] = d[9] = d[10] = d[11] = 0;
        }
        if (!d.empty()) tables.push_back({tags::HEAD().value, std::move(d)});
    }

    add_raw(tags::HHEA());

    // hmtx: keep only entries for retained glyphs.
    {
        auto d = extract_table(font, tags::HMTX());
        if (!d.empty()) {
            uint16_t n_hm = font.hhea.number_of_h_metrics;
            std::vector<uint8_t> new_hmtx;
            for (uint16_t new_gid = 0; new_gid < order.size(); ++new_gid) {
                uint16_t old_gid = order[new_gid];
                uint16_t aw = 0, lsb = 0;
                if (old_gid < n_hm) {
                    size_t off = size_t(old_gid) * 4;
                    if (off + 4 <= d.size()) {
                        aw  = uint16_t((d[off] << 8) | d[off+1]);
                        lsb = uint16_t((d[off+2] << 8) | d[off+3]);
                    }
                } else {
                    // monospace tail: last advance + per-glyph lsb.
                    if (n_hm > 0) {
                        size_t off = size_t(n_hm - 1) * 4;
                        if (off + 2 <= d.size()) aw = uint16_t((d[off] << 8) | d[off+1]);
                    }
                    size_t lsb_off = size_t(n_hm) * 4 + size_t(old_gid - n_hm) * 2;
                    if (lsb_off + 2 <= d.size())
                        lsb = uint16_t((d[lsb_off] << 8) | d[lsb_off+1]);
                }
                emit_u16(new_hmtx, aw);
                emit_u16(new_hmtx, lsb);
            }
            // Patch hhea.numberOfHMetrics.
            for (auto& t : tables) {
                if (t.tag == tags::HHEA().value && t.data.size() >= 36) {
                    t.data[34] = uint8_t(order.size() >> 8);
                    t.data[35] = uint8_t(order.size() & 0xFF);
                }
            }
            tables.push_back({tags::HMTX().value, std::move(new_hmtx)});
        }
    }

    // maxp: update numGlyphs.
    {
        auto d = extract_table(font, tags::MAXP());
        if (d.size() >= 6) {
            d[4] = uint8_t(order.size() >> 8);
            d[5] = uint8_t(order.size() & 0xFF);
        }
        if (!d.empty()) tables.push_back({tags::MAXP().value, std::move(d)});
    }

    // cmap: build minimal format-4 (requires codepoints, omit here — use empty).
    {
        std::vector<uint8_t> cmap_data;
        emit_u16(cmap_data, 0);  // version
        emit_u16(cmap_data, 1);  // numTables
        emit_u16(cmap_data, 3);  // platformId
        emit_u16(cmap_data, 1);  // encodingId
        emit_u32(cmap_data, 12); // offset
        // Format-4, empty (just terminator segment).
        emit_u16(cmap_data, 4);   // format
        emit_u16(cmap_data, 32);  // length
        emit_u16(cmap_data, 0);   // language
        emit_u16(cmap_data, 2);   // segCountX2
        emit_u16(cmap_data, 2);   // searchRange
        emit_u16(cmap_data, 0);   // entrySelector
        emit_u16(cmap_data, 0);   // rangeShift
        emit_u16(cmap_data, 0xFFFF); // endCode[0]
        emit_u16(cmap_data, 0);      // reservedPad
        emit_u16(cmap_data, 0xFFFF); // startCode[0]
        emit_u16(cmap_data, 1);      // idDelta[0]
        emit_u16(cmap_data, 0);      // idRangeOffset[0]
        tables.push_back({tags::CMAP().value, std::move(cmap_data)});
    }

    if (!new_loca.empty()) tables.push_back({tags::LOCA().value, std::move(new_loca)});
    if (!new_glyf.empty()) tables.push_back({tags::GLYF().value, std::move(new_glyf)});

    if (opts.retain_name) add_raw(tags::NAME());
    add_raw(tags::POST());
    if (opts.retain_kern) add_raw(tags::KERN());

    // Sort tables by tag value.
    std::sort(tables.begin(), tables.end(),
              [](const auto& a, const auto& b){ return a.tag < b.tag; });

    // Assemble SFNT.
    uint16_t n = uint16_t(tables.size());
    uint16_t sr = 1; int es = 0;
    while (sr * 2 <= n) { sr *= 2; ++es; }
    uint16_t rs = uint16_t(n - sr);

    std::vector<uint8_t>& sfnt = res.sfnt_data;
    emit_u32(sfnt, 0x00010000u);  // TrueType magic
    emit_u16(sfnt, n);
    emit_u16(sfnt, uint16_t(sr * 16));
    emit_u16(sfnt, uint16_t(es));
    emit_u16(sfnt, uint16_t(rs * 16));

    // Table directory (placeholder offsets).
    uint32_t data_offset = uint32_t(12 + n * 16);
    // Align data area.
    while (data_offset % 4) ++data_offset;

    for (const auto& t : tables) {
        uint32_t csum = sfnt_checksum(t.data.data(), t.data.size());
        emit_u32(sfnt, t.tag);
        emit_u32(sfnt, csum);
        emit_u32(sfnt, data_offset);
        emit_u32(sfnt, uint32_t(t.data.size()));
        data_offset += uint32_t((t.data.size() + 3) & ~3u);
    }

    // Append table data.
    for (const auto& t : tables) {
        sfnt.insert(sfnt.end(), t.data.begin(), t.data.end());
        pad_to_4(sfnt);
    }

    // Patch head.checksumAdjustment.
    uint32_t whole_sum = sfnt_checksum(sfnt.data(), sfnt.size());
    uint32_t adj = 0xB1B0AFBAu - whole_sum;
    // Find head table offset.
    for (size_t i = 0; i < n; ++i) {
        size_t entry = 12 + i * 16;
        uint32_t tag = (uint32_t(sfnt[entry])   << 24) |
                       (uint32_t(sfnt[entry+1]) << 16) |
                       (uint32_t(sfnt[entry+2]) <<  8) |
                        uint32_t(sfnt[entry+3]);
        if (tag == tags::HEAD().value) {
            uint32_t off = (uint32_t(sfnt[entry+8])  << 24) |
                           (uint32_t(sfnt[entry+9])  << 16) |
                           (uint32_t(sfnt[entry+10]) <<  8) |
                            uint32_t(sfnt[entry+11]);
            patch_u32(sfnt, off + 8, adj);
            break;
        }
    }

    res.ok = true;
    return res;
}

SubsetResult subset_font(
    const FontFace&              font,
    const std::vector<uint32_t>& codepoints,
    const SubsetOptions&         opts)
{
    std::unordered_set<uint16_t> gids;
    for (uint32_t cp : codepoints) {
        uint16_t gid = font.cmap.lookup(cp);
        if (gid != 0) gids.insert(gid);
    }
    return subset_font_by_gids(font, gids, opts);
}

} // namespace fontscope
