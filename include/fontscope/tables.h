#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/font_types.h"

namespace fontscope {

// Parsed head table (OpenType spec §head).
struct HeadTable {
    uint16_t major_version;
    uint16_t minor_version;
    Fixed16  font_revision;
    uint32_t checksum_adjustment;
    uint32_t magic_number;          // must be 0x5F0F3CF5
    uint16_t flags;
    uint16_t units_per_em;
    int64_t  created;
    int64_t  modified;
    FUnit    x_min, y_min, x_max, y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t  font_direction_hint;
    int16_t  index_to_loc_format;   // 0 = short offsets, 1 = long offsets
    int16_t  glyph_data_format;
};

// Parsed maxp table (OpenType spec §maxp).
struct MaxpTable {
    uint32_t version;               // 0x00005000 (v0.5) or 0x00010000 (v1.0)
    uint16_t num_glyphs;
    // v1.0 fields below; zero for v0.5
    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_composite_points;
    uint16_t max_composite_contours;
    uint16_t max_zones;
    uint16_t max_twilight_points;
    uint16_t max_storage;
    uint16_t max_function_defs;
    uint16_t max_instruction_defs;
    uint16_t max_stack_elements;
    uint16_t max_size_of_instructions;
    uint16_t max_component_elements;
    uint16_t max_component_depth;
};

// Parsed hhea table (OpenType spec §hhea).
struct HheaTable {
    uint16_t major_version;
    uint16_t minor_version;
    int16_t  ascender;
    int16_t  descender;
    int16_t  line_gap;
    uint16_t advance_width_max;
    int16_t  min_left_side_bearing;
    int16_t  min_right_side_bearing;
    int16_t  x_max_extent;
    int16_t  caret_slope_rise;
    int16_t  caret_slope_run;
    int16_t  caret_offset;
    int16_t  metric_data_format;
    uint16_t number_of_h_metrics;
};

// Parsed hmtx table (one entry per glyph; last LongHorMetric's advanceWidth
// extends to glyphs beyond number_of_h_metrics).
struct HmtxTable {
    std::vector<GlyphHMetrics> hmetrics;
    std::vector<int16_t>       left_side_bearings;
};

// Parsed post table (version, italic angle, underline position/thickness).
// For version 2.0 the glyph-name index and custom name strings are also kept.
struct PostTable {
    Fixed16  version;
    Fixed16  italic_angle;
    int16_t  underline_position;
    int16_t  underline_thickness;
    uint32_t is_fixed_pitch;
    uint32_t min_mem_type42;
    uint32_t max_mem_type42;
    uint32_t min_mem_type1;
    uint32_t max_mem_type1;

    // Version 2.0 glyph-name table (empty for other versions).
    // name_index[gid] < 258 selects a standard Macintosh name; values >= 258
    // index custom_names[name_index[gid] - 258].
    uint16_t                 num_names{0};
    std::vector<uint16_t>    name_index;
    std::vector<std::string> custom_names;
};

// OS/2 table (selected fields used for metrics).
struct Os2Table {
    uint16_t version;
    int16_t  x_avg_char_width;
    uint16_t us_weight_class;
    uint16_t us_width_class;
    uint16_t fs_type;
    int16_t  y_subscript_x_size;
    int16_t  y_subscript_y_size;
    int16_t  y_subscript_x_offset;
    int16_t  y_subscript_y_offset;
    int16_t  y_superscript_x_size;
    int16_t  y_superscript_y_size;
    int16_t  y_superscript_x_offset;
    int16_t  y_superscript_y_offset;
    int16_t  y_strikeout_size;
    int16_t  y_strikeout_position;
    int16_t  s_family_class;
    uint8_t  panose[10];
    uint32_t ul_unicode_range[4];
    uint8_t  ach_vend_id[4];
    uint16_t fs_selection;
    uint16_t us_first_char_index;
    uint16_t us_last_char_index;
    int16_t  s_typo_ascender;
    int16_t  s_typo_descender;
    int16_t  s_typo_line_gap;
    uint16_t us_win_ascent;
    uint16_t us_win_descent;
};

Result<HeadTable> parse_head(ByteReader& r);
Result<MaxpTable> parse_maxp(ByteReader& r);
Result<HheaTable> parse_hhea(ByteReader& r);
Result<HmtxTable> parse_hmtx(ByteReader& r, uint16_t num_glyphs,
                               uint16_t number_of_h_metrics);
Result<PostTable> parse_post(ByteReader& r);
Result<Os2Table>  parse_os2(ByteReader& r);

// Retrieve horizontal metrics for a single glyph.
GlyphHMetrics get_glyph_hmetrics(const HmtxTable& hmtx, uint16_t glyph_id);

// Resolve the PostScript glyph name for a glyph id from a version 2.0 post
// table. Returns a standard Macintosh name for indices below 258 and the
// font-supplied custom name otherwise. Empty string when no name table is
// present.
std::string post_glyph_name(const PostTable& post, uint16_t glyph_id);

} // namespace fontscope
