#include "fontscope/kern.h"
#include <algorithm>

namespace fontscope {

int16_t KernTable::lookup(uint16_t left, uint16_t right) const {
    for (const auto& sub : subtables) {
        // Only use horizontal kerning subtables (coverage bit 0 set).
        if (!(sub.coverage & 1)) continue;

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
        } else {
            // Skip unsupported kern subtable formats.
            if (length > 6) r.skip(length - 6);
        }

        kern.subtables.push_back(std::move(sub));
    }

    return Result<KernTable>::success(std::move(kern));
}

} // namespace fontscope
