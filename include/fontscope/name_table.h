#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"

namespace fontscope {

struct NameRecord {
    uint16_t    platform_id;
    uint16_t    encoding_id;
    uint16_t    language_id;
    uint16_t    name_id;
    std::string value;
};

struct NameTable {
    std::vector<NameRecord> records;

    // Find the first record matching name_id (prefers platform 3, any language).
    const std::string* find(uint16_t name_id) const;
};

// Well-known name IDs per the OpenType spec.
enum class NameId : uint16_t {
    Copyright      = 0,
    FamilyName     = 1,
    SubfamilyName  = 2,
    UniqueId       = 3,
    FullName       = 4,
    Version        = 5,
    PostScriptName = 6,
    Trademark      = 7,
    Manufacturer   = 8,
    Designer       = 9,
    Description    = 10,
    VendorUrl      = 11,
    DesignerUrl    = 12,
    LicenseDesc    = 13,
    LicenseUrl     = 14,
    TypoFamilyName = 16,
    TypoSubfamily  = 17,
    SampleText     = 19,
};

Result<NameTable> parse_name(ByteReader& r);

} // namespace fontscope
