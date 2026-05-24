#pragma once
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include "fontscope/inspect.h"

namespace fontscope {

// Options controlling which tables are carried into the subset.
struct SubsetOptions {
    bool retain_hints        = false;  // drop hinting instructions to save space
    bool retain_kern         = true;   // keep kern table for retained pairs
    bool retain_name         = true;   // keep name records
    bool retain_var_tables   = false;  // keep gvar/fvar/avar
    bool retain_color_tables = false;  // keep COLR/CPAL
    bool retain_layout_tables = false; // keep GSUB/GPOS
};

// Result of a subset operation.
struct SubsetResult {
    std::vector<uint8_t>              sfnt_data;     // raw output font bytes
    std::unordered_map<uint16_t,uint16_t> gid_map;  // old GID → new GID
    uint16_t                          num_glyphs;
    bool                              ok;
    std::string                       error_msg;
};

// Subset a font to only the glyphs needed to render the given Unicode codepoints.
// Composite glyphs pull in their component glyphs transitively.
SubsetResult subset_font(
    const FontFace&                       font,
    const std::vector<uint32_t>&          codepoints,
    const SubsetOptions&                  opts = {});

// Subset by explicit glyph IDs (already resolved; components fetched transitively).
SubsetResult subset_font_by_gids(
    const FontFace&                       font,
    const std::unordered_set<uint16_t>&   glyph_ids,
    const SubsetOptions&                  opts = {});

// Collect all component GIDs transitively referenced by a composite glyph.
void collect_component_gids(
    const FontFace&                font,
    uint16_t                       glyph_id,
    std::unordered_set<uint16_t>&  out);

// Build a new glyph-order vector (sorted by original GID) and a GID remap table.
std::vector<uint16_t> build_glyph_order(const std::unordered_set<uint16_t>& gids);
std::unordered_map<uint16_t,uint16_t> build_gid_map(const std::vector<uint16_t>& order);

// Serialization helpers for subset SFNT tables.
// Emit a big-endian 16-bit value.
void emit_u16(std::vector<uint8_t>& out, uint16_t v);
// Emit a big-endian 32-bit value.
void emit_u32(std::vector<uint8_t>& out, uint32_t v);
// Patch a big-endian u32 at a specific offset.
void patch_u32(std::vector<uint8_t>& out, size_t offset, uint32_t v);
// Compute SFNT table checksum (sum of big-endian u32 words, padded to 4 bytes).
uint32_t sfnt_checksum(const uint8_t* data, size_t len);
// Align a byte vector to a 4-byte boundary by appending zero padding.
void pad_to_4(std::vector<uint8_t>& out);

} // namespace fontscope
