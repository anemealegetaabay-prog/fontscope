#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "fontscope/inspect.h"

namespace fontscope {

// Category of difference between two fonts.
enum class DiffKind : uint8_t {
    TableMissing,        // table present in A, absent in B
    TableAdded,          // table absent in A, present in B
    TableChanged,        // table present in both but different checksum or length
    MetricMismatch,      // head/hhea/maxp field differs
    GlyphCountChanged,   // maxp.numGlyphs differs
    NameChanged,         // name table record changed
    AxisChanged,         // fvar axis range or default changed
    ChecksumError,       // table checksum mismatch
    UpmChanged,          // units_per_em changed
};

// A single diff entry.
struct FontDiffEntry {
    DiffKind    kind;
    std::string location;   // e.g. "head.unitsPerEm", "table:GLYF", "name:4"
    std::string value_a;    // value in font A (empty if not applicable)
    std::string value_b;    // value in font B
};

// Full diff result.
struct FontDiff {
    std::vector<FontDiffEntry> entries;
    bool identical;
    int  n_errors;
    int  n_warnings;
    int  n_info;
};

// Compare two loaded fonts and return a structured diff.
FontDiff diff_fonts(const FontFace& a, const FontFace& b);

// Return true if two fonts are byte-for-byte identical in all shared tables.
bool fonts_identical(const FontFace& a, const FontFace& b);

// Print a human-readable diff report to stdout.
void print_font_diff(const FontDiff& diff);

// Helpers for individual table comparison.
bool compare_head(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out);
bool compare_hhea(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out);
bool compare_maxp(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out);
bool compare_name(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out);
bool compare_fvar(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out);
bool compare_table_checksums(const FontFace& a, const FontFace& b,
                              std::vector<FontDiffEntry>& out);

} // namespace fontscope
