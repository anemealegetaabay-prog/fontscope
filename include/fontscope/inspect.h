#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "fontscope/errors.h"
#include "fontscope/sfnt.h"
#include "fontscope/tables.h"
#include "fontscope/cmap.h"
#include "fontscope/loca.h"
#include "fontscope/glyf.h"
#include "fontscope/name_table.h"
#include "fontscope/colr.h"
#include "fontscope/cpal.h"
#include "fontscope/fvar.h"
#include "fontscope/avar.h"
#include "fontscope/variation.h"

namespace fontscope {

// All parsed font state in one place.
struct FontFace {
    std::vector<uint8_t> raw_data;

    SfntHeader  sfnt;
    HeadTable   head;
    MaxpTable   maxp;
    HheaTable   hhea;
    HmtxTable   hmtx;
    PostTable   post;
    Os2Table    os2;
    NameTable   name;
    CmapIndex   cmap;
    LocaTable   loca;
    ColrTable   colr;
    CpalTable   cpal;
    FvarTable   fvar;
    AvarTable   avar;

    bool has_os2{false};
    bool has_name{false};
    bool has_colr{false};
    bool has_cpal{false};
    bool has_fvar{false};
    bool has_avar{false};
};

// Load and parse a font from raw bytes.
Result<FontFace> load_font(const uint8_t* data, size_t size);

// Print a human-readable font summary to stdout.
void print_font_info(const FontFace& font);

// Print all table records with offsets, lengths, and checksum status.
void print_tables(const FontFace& font);

// Load and return a parsed glyph outline.
Result<RawGlyph> load_glyph(const FontFace& font, uint16_t glyph_id);

// Resolve a Unicode code point to a glyph ID.
uint16_t codepoint_to_glyph(const FontFace& font, uint32_t codepoint);

} // namespace fontscope
