#include "fontscope/sfnt.h"
#include <algorithm>

namespace fontscope {

bool sfnt_version_is_valid(uint32_t v) {
    return v == kSfntVersionTrueType
        || v == kSfntVersionCFF
        || v == kSfntVersionTrue
        || v == kSfntVersionTyp1;
}

const char* sfnt_version_name(uint32_t v) {
    switch (v) {
    case kSfntVersionTrueType: return "TrueType 1.0";
    case kSfntVersionCFF:      return "CFF/OTTO";
    case kSfntVersionTrue:     return "TrueType (Apple)";
    case kSfntVersionTyp1:     return "Type 1 (old Apple)";
    default:                   return "unknown";
    }
}

Result<SfntHeader> parse_sfnt_header(ByteReader& r) {
    SfntHeader hdr{};

    hdr.sfVersion    = r.read_u32_be();
    hdr.numTables    = r.read_u16_be();
    hdr.searchRange  = r.read_u16_be();
    hdr.entrySelector= r.read_u16_be();
    hdr.rangeShift   = r.read_u16_be();

    if (!r.ok())
        return Result<SfntHeader>::error(Status::TruncatedInput);

    if (!sfnt_version_is_valid(hdr.sfVersion))
        return Result<SfntHeader>::error(Status::InvalidMagic);

    if (hdr.numTables == 0 || hdr.numTables > 256)
        return Result<SfntHeader>::error(Status::MalformedTable);

    hdr.tables.reserve(hdr.numTables);

    for (uint16_t i = 0; i < hdr.numTables; ++i) {
        TableRecord rec{};
        uint32_t raw_tag = r.read_u32_be();
        rec.tag.value    = raw_tag;
        rec.checksum     = r.read_u32_be();
        rec.offset       = r.read_u32_be();
        rec.length       = r.read_u32_be();

        if (!r.ok())
            return Result<SfntHeader>::error(Status::TruncatedInput);

        hdr.tables.push_back(rec);
    }

    return Result<SfntHeader>::success(std::move(hdr));
}

const TableRecord* find_table(const SfntHeader& hdr, Tag tag) {
    for (const auto& t : hdr.tables)
        if (t.tag == tag) return &t;
    return nullptr;
}

bool verify_table_checksum(const TableRecord& rec,
                           const uint8_t* file_data, size_t file_size)
{
    if (rec.offset > file_size) return false;
    size_t avail = file_size - rec.offset;
    size_t len   = std::min(size_t(rec.length), avail);

    // head table has a checksumAdjustment field at offset +8 that must be
    // treated as zero when computing the directory checksum.
    bool is_head = (rec.tag == tags::HEAD());

    uint32_t sum = 0;
    const uint8_t* p = file_data + rec.offset;
    size_t full_words = len / 4;

    for (size_t i = 0; i < full_words; ++i) {
        uint32_t w = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
                   | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
        if (is_head && (p - (file_data + rec.offset)) == 8)
            w = 0;  // checksumAdjustment field
        sum += w;
        p += 4;
    }

    // Tail bytes (if length is not a multiple of 4).
    if (len & 3) {
        uint32_t tail = 0;
        for (size_t k = 0; k < (len & 3); ++k)
            tail |= uint32_t(p[k]) << (24 - k * 8);
        sum += tail;
    }

    return sum == rec.checksum;
}

} // namespace fontscope
