#pragma once
#include <cstdint>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/font_types.h"

namespace fontscope {

// OpenType/TrueType SFNT version values.
static constexpr uint32_t kSfntVersionTrueType  = 0x00010000u;
static constexpr uint32_t kSfntVersionCFF       = 0x4F54544Fu;  // 'OTTO'
static constexpr uint32_t kSfntVersionTrue       = 0x74727565u;  // 'true' (Apple)
static constexpr uint32_t kSfntVersionTyp1      = 0x74797031u;  // 'typ1'
static constexpr uint32_t kSfntVersionTTC       = 0x74746366u;  // 'ttcf'

// One entry in the SFNT table directory.
struct TableRecord {
    Tag      tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

// The top-level SFNT offset table and table directory.
struct SfntHeader {
    uint32_t              sfVersion;   // one of the kSfntVersion* constants
    uint16_t              numTables;
    uint16_t              searchRange;
    uint16_t              entrySelector;
    uint16_t              rangeShift;
    std::vector<TableRecord> tables;
};

// Parse the SFNT offset table + table directory from the start of the reader.
Result<SfntHeader> parse_sfnt_header(ByteReader& r);

// Find a table by tag in the directory. Returns nullptr if not found.
const TableRecord* find_table(const SfntHeader& hdr, Tag tag);

// Verify the stored checksum for a table against the raw file bytes.
bool verify_table_checksum(const TableRecord& rec,
                           const uint8_t* file_data, size_t file_size);

// True if sfVersion is a recognized OpenType/TrueType value.
bool sfnt_version_is_valid(uint32_t v);

// Human-readable string for an sfnt version (e.g. "TrueType 1.0", "CFF/OTTO").
const char* sfnt_version_name(uint32_t v);

} // namespace fontscope
