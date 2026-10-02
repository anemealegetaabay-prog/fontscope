#include "fontscope/cmap.h"
#include <algorithm>

namespace fontscope {

// Format 4: segmented coverage, the most common Windows cmap.
static void parse_format4(ByteReader& r, CmapIndex& idx) {
    uint16_t length         = r.read_u16_be();
    r.skip(2);  // language
    uint16_t seg_count_x2  = r.read_u16_be();
    r.skip(6);  // searchRange, entrySelector, rangeShift

    if (!r.ok() || seg_count_x2 < 2 || (seg_count_x2 & 1)) return;
    uint16_t seg_count = seg_count_x2 / 2;

    std::vector<uint16_t> end_code(seg_count);
    for (auto& v : end_code) v = r.read_u16_be();
    r.skip(2);  // reservedPad
    std::vector<uint16_t> start_code(seg_count);
    for (auto& v : start_code) v = r.read_u16_be();
    std::vector<int16_t> id_delta(seg_count);
    for (auto& v : id_delta) v = r.read_i16_be();

    // Remember position of idRangeOffset array for relative lookups.
    size_t range_offset_base = r.pos();
    std::vector<uint16_t> id_range_offset(seg_count);
    for (auto& v : id_range_offset) v = r.read_u16_be();

    if (!r.ok()) return;

    for (uint16_t s = 0; s < seg_count; ++s) {
        if (start_code[s] == 0xFFFF) break;  // terminator segment
        for (uint32_t cp = start_code[s]; cp <= end_code[s]; ++cp) {
            uint16_t glyph = 0;
            if (id_range_offset[s] == 0) {
                glyph = uint16_t(int32_t(cp) + id_delta[s]);
            } else {
                // Pointer into glyphIdArray: idRangeOffset[s] is relative to
                // the address of idRangeOffset[s] itself.
                size_t glyph_ptr = range_offset_base + s * 2
                                 + id_range_offset[s]
                                 + (cp - start_code[s]) * 2;
                ByteReader pr = r.sub_reader(glyph_ptr, 2);
                uint16_t v = pr.read_u16_be();
                if (pr.ok() && v != 0)
                    glyph = uint16_t(int32_t(v) + id_delta[s]);
            }
            if (glyph != 0)
                idx.unicode_to_glyph[cp] = glyph;
        }
    }
    (void)length;
}

// Format 0: byte-range Apple Roman encoding (rarely used but easy).
static void parse_format0(ByteReader& r, CmapIndex& idx) {
    r.skip(4);  // length, language
    for (uint16_t i = 0; i < 256; ++i) {
        uint8_t g = r.read_u8();
        if (g != 0) idx.unicode_to_glyph[i] = g;
    }
}

// Format 6: trimmed table mapping.
static void parse_format6(ByteReader& r, CmapIndex& idx) {
    r.skip(4);  // length, language
    uint16_t first = r.read_u16_be();
    uint16_t count = r.read_u16_be();
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t g = r.read_u16_be();
        if (!r.ok()) break;
        if (g != 0) idx.unicode_to_glyph[uint32_t(first) + i] = g;
    }
}

// Format 12: segmented coverage for full Unicode (BMP + SMP).
static void parse_format12(ByteReader& r, CmapIndex& idx) {
    r.skip(2);  // reserved
    r.skip(4);  // length
    r.skip(4);  // language
    uint32_t n_groups = r.read_u32_be();
    if (!r.ok() || n_groups > 0x10000) return;

    for (uint32_t g = 0; g < n_groups; ++g) {
        uint32_t start_char_code  = r.read_u32_be();
        uint32_t end_char_code    = r.read_u32_be();
        uint32_t start_glyph_id   = r.read_u32_be();
        if (!r.ok()) break;
        if (end_char_code < start_char_code) continue;
        uint32_t span = end_char_code - start_char_code;
        for (uint32_t i = 0; i <= span; ++i) {
            uint32_t cp = start_char_code + i;
            uint32_t gid = start_glyph_id + i;
            if (gid <= 0xFFFF)
                idx.unicode_to_glyph[cp] = uint16_t(gid);
        }
    }
}

Result<CmapIndex> parse_cmap(ByteReader& r) {
    size_t table_start = r.pos();

    uint16_t version   = r.read_u16_be();
    uint16_t num_tables= r.read_u16_be();
    if (!r.ok()) return Result<CmapIndex>::error(Status::TruncatedInput);
    (void)version;

    CmapIndex idx;

    for (uint16_t i = 0; i < num_tables; ++i) {
        CmapSubtable st{};
        st.platform_id = r.read_u16_be();
        st.encoding_id = r.read_u16_be();
        st.offset      = r.read_u32_be();
        if (!r.ok()) break;
        // Every subtable starts with its uint16 format; 0 if out of bounds.
        ByteReader fr = r.sub_reader(table_start + st.offset, 2);
        st.format = fr.read_u16_be();
        idx.subtables.push_back(st);
    }

    // Prefer Unicode BMP (platform 3, encoding 1) or Unicode full (3, 10).
    // Fall back to platform 0 (Unicode) subtables.
    const CmapSubtable* best = nullptr;
    for (const auto& st : idx.subtables) {
        if (st.platform_id == 3 && st.encoding_id == 10) { best = &st; break; }
    }
    if (!best) {
        for (const auto& st : idx.subtables) {
            if (st.platform_id == 3 && st.encoding_id == 1) { best = &st; break; }
        }
    }
    if (!best) {
        for (const auto& st : idx.subtables) {
            if (st.platform_id == 0) { best = &st; break; }
        }
    }
    if (!best && !idx.subtables.empty())
        best = &idx.subtables[0];

    if (best) {
        ByteReader sub = r.sub_reader(table_start + best->offset,
                                       r.size() - (table_start + best->offset));
        uint16_t fmt = sub.read_u16_be();
        if (sub.ok()) {
            switch (fmt) {
            case 0:  parse_format0(sub, idx);  break;
            case 4:  parse_format4(sub, idx);  break;
            case 6:  parse_format6(sub, idx);  break;
            case 12: parse_format12(sub, idx); break;
            default: break;
            }
        }
    }

    return Result<CmapIndex>::success(std::move(idx));
}

} // namespace fontscope
