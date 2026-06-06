#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace fontscope {

// A Unicode block: a named contiguous range of code points.
struct UnicodeBlock {
    uint32_t    first;   // first code point in block
    uint32_t    last;    // last code point in block (inclusive)
    const char* name;    // block name as in Unicode standard
    const char* abbrev;  // short abbreviation for display
};

// Return the full Unicode block table (a subset of Unicode 15.0 blocks).
const std::vector<UnicodeBlock>& all_unicode_blocks();

// Return the block containing a code point, or nullopt if unassigned.
std::optional<const UnicodeBlock*> block_for_codepoint(uint32_t cp);

// Return the block name string for a code point, or "Unknown" if none.
const char* block_name(uint32_t cp);

// Collect all code points from a set that fall within a specific block.
std::vector<uint32_t> filter_by_block(
    const std::vector<uint32_t>& codepoints,
    const UnicodeBlock& block);

// Count how many code points in the set fall within each block.
// Returns a vector of (block_index, count) pairs, sorted by count descending.
struct BlockUsage {
    size_t   block_index;
    uint32_t count;
    const UnicodeBlock* block;
};

std::vector<BlockUsage> count_by_block(const std::vector<uint32_t>& codepoints);

// Return the set of Unicode blocks that are covered by a font's cmap.
// A block is "covered" if the font has at least `min_coverage` fraction
// of its code points mapped to non-zero GIDs.
struct BlockCoverage {
    const UnicodeBlock* block;
    uint32_t   total_in_block;    // code points in the block range
    uint32_t   mapped_in_font;    // code points with non-zero GID
    double     coverage;          // mapped / total
};

} // namespace fontscope
