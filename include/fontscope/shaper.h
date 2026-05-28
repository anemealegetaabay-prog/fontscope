#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "fontscope/inspect.h"
#include "fontscope/cmap.h"
#include "fontscope/kern.h"

namespace fontscope {

// OpenType script/language tags (4-byte ASCII, big-endian uint32).
namespace script_tags {
    constexpr uint32_t DFLT = 0x44464C54;
    constexpr uint32_t latn = 0x6C61746E;
    constexpr uint32_t cyrl = 0x6379726C;
    constexpr uint32_t grek = 0x6772656B;
    constexpr uint32_t arab = 0x61726162;
    constexpr uint32_t hebr = 0x68656272;
    constexpr uint32_t thai = 0x74686169;
    constexpr uint32_t kana = 0x6B616E61;
    constexpr uint32_t hani = 0x68616E69;
    constexpr uint32_t deva = 0x64657661;
}

namespace lang_tags {
    constexpr uint32_t dflt = 0x00000000;  // default language
    constexpr uint32_t ENG  = 0x454E4720;
    constexpr uint32_t FRA  = 0x46524120;
    constexpr uint32_t DEU  = 0x44455520;
    constexpr uint32_t ARA  = 0x41524120;
    constexpr uint32_t HEB  = 0x48454220;
    constexpr uint32_t TRK  = 0x54524B20;
    constexpr uint32_t RUS  = 0x52555320;
}

// OpenType feature tags (subset of common features).
namespace feature_tags {
    constexpr uint32_t calt = 0x63616C74;  // Contextual alternates
    constexpr uint32_t ccmp = 0x63636D70;  // Glyph composition/decomposition
    constexpr uint32_t clig = 0x636C6967;  // Contextual ligatures
    constexpr uint32_t kern = 0x6B65726E;  // Kerning
    constexpr uint32_t liga = 0x6C696761;  // Standard ligatures
    constexpr uint32_t lnum = 0x6C6E756D;  // Lining figures
    constexpr uint32_t locl = 0x6C6F636C;  // Localized forms
    constexpr uint32_t mark = 0x6D61726B;  // Mark positioning
    constexpr uint32_t mkmk = 0x6D6B6D6B;  // Mark-to-mark
    constexpr uint32_t onum = 0x6F6E756D;  // Oldstyle figures
    constexpr uint32_t pnum = 0x706E756D;  // Proportional figures
    constexpr uint32_t smcp = 0x736D6370;  // Small capitals
    constexpr uint32_t sups = 0x73757073;  // Superscript
    constexpr uint32_t tnum = 0x746E756D;  // Tabular figures
    constexpr uint32_t zero = 0x7A65726F;  // Slashed zero
}

// A single shaped glyph with positioning information.
struct ShapedGlyph {
    uint16_t glyph_id;
    uint32_t cluster;    // index into source codepoint array
    int32_t  x_advance;  // in font units
    int32_t  y_advance;
    int32_t  x_offset;   // positioning offset
    int32_t  y_offset;
};

// Result of shaping a sequence of codepoints.
struct ShapeResult {
    std::vector<ShapedGlyph> glyphs;
    bool rtl;              // true if shaped right-to-left
    uint32_t script_tag;
    uint32_t lang_tag;
};

// Options for the shaper.
struct ShapeOptions {
    uint32_t script_tag   = script_tags::latn;
    uint32_t lang_tag     = lang_tags::ENG;
    bool     rtl          = false;
    bool     apply_kern   = true;
    bool     apply_liga   = true;
    bool     apply_calt   = false;  // requires GSUB, only for future use
    uint16_t ppem         = 0;      // 0 = return in font units
};

// Simple shape: map codepoints to glyphs and apply kerning from the kern table.
// Does not require GSUB/GPOS — works with any font that has kern and cmap.
ShapeResult shape_simple(
    const FontFace&              font,
    const std::vector<uint32_t>& codepoints,
    const ShapeOptions&          opts = {});

// Apply a kern table to a shaped glyph sequence (mutates x_advance).
void apply_kern_to_glyphs(
    std::vector<ShapedGlyph>& glyphs,
    const KernTable&          kern);

// Convert ShapeResult to total advance width (sum of x_advances + x_offsets).
int32_t total_advance(const ShapeResult& result);

// Cluster iterator: groups glyphs sharing a cluster index.
// Returns spans [start, end) for each cluster.
std::vector<std::pair<size_t,size_t>>
cluster_spans(const std::vector<ShapedGlyph>& glyphs);

// Reverse a glyph sequence for RTL output.
void reverse_glyphs(std::vector<ShapedGlyph>& glyphs);

// Simple GSUB single-substitution lookup (format 1 and 2).
// Returns the substituted GID, or 0 if no substitution applies.
// `coverage` maps input GID → coverage index; `subst` maps coverage index → output GID.
uint16_t gsub_single_subst(
    uint16_t glyph_id,
    const std::unordered_map<uint16_t,uint16_t>& coverage,
    const std::vector<uint16_t>&                 subst);

// Simple GSUB ligature substitution: try to match a ligature starting at position
// `pos` in the glyph sequence. Returns the new GID and consumed glyph count on
// success, or {0, 0} on miss.
struct LigatureMatch {
    uint16_t output_glyph;
    size_t   n_consumed;
};

struct LigatureSet {
    std::vector<uint16_t>             sequence;  // component GIDs (excluding first)
    uint16_t                          output_glyph;
};

LigatureMatch try_ligature(
    const std::vector<ShapedGlyph>& glyphs,
    size_t                          pos,
    const std::vector<LigatureSet>& sets);

// Glyph class enumeration (used in GSUB/GPOS class-based rules).
enum class GlyphClass : uint8_t {
    Base    = 1,
    Ligature = 2,
    Mark    = 3,
    Component = 4,
};

// Determine script for a codepoint using Unicode block ranges.
uint32_t codepoint_script(uint32_t cp);

// Heuristic: detect whether a string is likely RTL.
bool text_likely_rtl(const std::vector<uint32_t>& codepoints);

} // namespace fontscope
