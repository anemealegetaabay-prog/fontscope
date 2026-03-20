#include "fontscope/name_table.h"
#include <algorithm>

namespace fontscope {

const std::string* NameTable::find(uint16_t name_id) const {
    // Prefer Windows Unicode (platform 3, encoding 1) English names first.
    for (const auto& r : records)
        if (r.name_id == name_id && r.platform_id == 3 && r.encoding_id == 1
            && (r.language_id == 0x0409 || r.language_id == 0))
            return &r.value;
    // Fall back to any matching record.
    for (const auto& r : records)
        if (r.name_id == name_id)
            return &r.value;
    return nullptr;
}

// Decode a UTF-16BE string to UTF-8.
static std::string utf16be_to_utf8(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len / 2);
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint32_t cp = (uint32_t(data[i]) << 8) | data[i+1];
        if (cp < 0x80) {
            out.push_back(char(cp));
        } else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

Result<NameTable> parse_name(ByteReader& r) {
    size_t table_start = r.pos();

    uint16_t format  = r.read_u16_be();
    uint16_t count   = r.read_u16_be();
    uint16_t storage_offset = r.read_u16_be();
    if (!r.ok()) return Result<NameTable>::error(Status::TruncatedInput);
    (void)format;

    NameTable name;
    name.records.reserve(count);

    struct RawRecord {
        uint16_t platform_id, encoding_id, language_id, name_id;
        uint16_t length, offset;
    };

    std::vector<RawRecord> raws(count);
    for (auto& rr : raws) {
        rr.platform_id = r.read_u16_be();
        rr.encoding_id = r.read_u16_be();
        rr.language_id = r.read_u16_be();
        rr.name_id     = r.read_u16_be();
        rr.length      = r.read_u16_be();
        rr.offset      = r.read_u16_be();
        if (!r.ok()) return Result<NameTable>::error(Status::TruncatedInput);
    }

    for (const auto& rr : raws) {
        size_t abs_off = table_start + storage_offset + rr.offset;
        ByteReader str_r = r.sub_reader(abs_off, rr.length);
        if (!str_r.ok()) continue;

        std::vector<uint8_t> buf(rr.length);
        str_r.read_bytes(buf.data(), rr.length);
        if (!str_r.ok()) continue;

        NameRecord nr;
        nr.platform_id = rr.platform_id;
        nr.encoding_id = rr.encoding_id;
        nr.language_id = rr.language_id;
        nr.name_id     = rr.name_id;

        // Platform 0 (Unicode) or 3 (Windows) use UTF-16BE encoding.
        if (rr.platform_id == 0 || rr.platform_id == 3)
            nr.value = utf16be_to_utf8(buf.data(), buf.size());
        else
            nr.value = std::string(buf.begin(), buf.end());

        name.records.push_back(std::move(nr));
    }

    return Result<NameTable>::success(std::move(name));
}

} // namespace fontscope
