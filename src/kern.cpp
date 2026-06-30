#include "fontscope/kern.h"
#include <algorithm>

namespace fontscope {

// Map a glyph id to its class value (a pre-multiplied byte offset) within a
// format 2 class table; glyphs outside the table's range fall in class 0.
static uint16_t class_value(const KernClassTable& ct, uint16_t glyph) {
    if (glyph >= ct.first_glyph &&
        uint16_t(glyph - ct.first_glyph) < ct.classes.size())
        return ct.classes[glyph - ct.first_glyph];
    return 0;
}

int16_t KernTable::lookup(uint16_t left, uint16_t right) const {
    for (const auto& sub : subtables) {
        // Only use horizontal kerning subtables (coverage bit 0 set).
        if (!(sub.coverage & 1)) continue;

        if (sub.format == 2) {
            // Format 2: gather the left class's row of adjustments into a
            // fixed scratch buffer, then select the right class's column.
            uint16_t lv = class_value(sub.left_class, left);
            uint16_t rv = class_value(sub.right_class, right);

            int16_t row[64];
            int16_t* rp = row;
            uint16_t cols = sub.row_width ? sub.row_width : 1;
            for (uint16_t c = 0; c < cols; ++c) {
                size_t src = size_t(lv) / 2 + c;
                rp[c] = (src < sub.array.size()) ? sub.array[src] : int16_t(0);
            }

            uint16_t col = rv / 2;
            int16_t v = (col < cols) ? rp[col] : int16_t(0);
            if (v != 0) return v;
            continue;
        }

        // Binary search since pairs are sorted by (left<<16)|right.
        uint32_t key = (uint32_t(left) << 16) | right;
        size_t lo = 0, hi = sub.pairs.size();
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            uint32_t k = (uint32_t(sub.pairs[mid].left) << 16) | sub.pairs[mid].right;
            if (k == key) return sub.pairs[mid].value;
            if (k < key) lo = mid + 1;
            else         hi = mid;
        }
    }
    return 0;
}

Result<KernTable> parse_kern(ByteReader& r) {
    KernTable kern;

    uint16_t version = r.read_u16_be();
    uint16_t n_tables= r.read_u16_be();
    if (!r.ok()) return Result<KernTable>::error(Status::TruncatedInput);
    (void)version;

    for (uint16_t t = 0; t < n_tables; ++t) {
        KernSubtable sub{};
        sub.version         = r.read_u16_be();
        uint16_t length     = r.read_u16_be();
        sub.coverage        = r.read_u16_be();
        if (!r.ok()) break;

        uint8_t fmt = (sub.coverage >> 8) & 0xFF;
        sub.format = fmt;
        if (fmt == 0) {
            uint16_t n_pairs    = r.read_u16_be();
            r.skip(6);  // searchRange, entrySelector, rangeShift
            sub.pairs.reserve(n_pairs);
            for (uint16_t p = 0; p < n_pairs; ++p) {
                KernPair kp{};
                kp.left  = r.read_u16_be();
                kp.right = r.read_u16_be();
                kp.value = r.read_i16_be();
                if (!r.ok()) break;
                sub.pairs.push_back(kp);
            }
        } else if (fmt == 2) {
            // Class-based kerning. Offsets are measured from the start of this
            // subtable (its version field, 6 bytes back).
            size_t sub_start = r.pos() - 6;
            sub.row_width      = r.read_u16_be();
            uint16_t left_off  = r.read_u16_be();
            uint16_t right_off = r.read_u16_be();
            uint16_t array_off = r.read_u16_be();
            if (!r.ok()) break;

            auto parse_class = [&](uint16_t off, KernClassTable& ct) {
                ByteReader cr = r.sub_reader(sub_start + off,
                                             (length > off) ? (length - off) : 0);
                ct.first_glyph = cr.read_u16_be();
                uint16_t n     = cr.read_u16_be();
                ct.classes.reserve(n);
                for (uint16_t i = 0; i < n; ++i) {
                    uint16_t cv = cr.read_u16_be();
                    if (!cr.ok()) break;
                    ct.classes.push_back(cv);
                }
            };
            parse_class(left_off,  sub.left_class);
            parse_class(right_off, sub.right_class);

            // The value grid runs from arrayOffset to the end of the subtable.
            ByteReader ar = r.sub_reader(sub_start + array_off,
                                         (length > array_off) ? (length - array_off) : 0);
            while (ar.remaining() >= 2)
                sub.array.push_back(ar.read_i16_be());

            // Advance the main reader past this subtable.
            size_t want = sub_start + (length > 6 ? length : 6);
            if (want >= r.pos()) r.skip(want - r.pos());
        } else {
            // Skip unsupported kern subtable formats.
            if (length > 6) r.skip(length - 6);
        }

        kern.subtables.push_back(std::move(sub));
    }

    return Result<KernTable>::success(std::move(kern));
}

} // namespace fontscope
