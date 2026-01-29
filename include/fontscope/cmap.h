#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

// A parsed cmap subtable descriptor.
struct CmapSubtable {
    uint16_t platform_id;
    uint16_t encoding_id;
    uint16_t format;
    uint32_t offset;  // relative to start of cmap table
};

// The full parsed cmap: subtable metadata + a flattened Unicode-to-glyph map.
struct CmapIndex {
    std::vector<CmapSubtable>          subtables;
    std::unordered_map<uint32_t,uint16_t> unicode_to_glyph;

    uint16_t lookup(uint32_t codepoint) const {
        auto it = unicode_to_glyph.find(codepoint);
        return it != unicode_to_glyph.end() ? it->second : 0;
    }
};

Result<CmapIndex> parse_cmap(ByteReader& r);

} // namespace fontscope
