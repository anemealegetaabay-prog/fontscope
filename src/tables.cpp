#include "fontscope/tables.h"
#include <algorithm>

namespace fontscope {

Result<HeadTable> parse_head(ByteReader& r) {
    HeadTable h{};
    h.major_version      = r.read_u16_be();
    h.minor_version      = r.read_u16_be();
    h.font_revision      = Fixed16::from_raw(r.read_i32_be());
    h.checksum_adjustment= r.read_u32_be();
    h.magic_number       = r.read_u32_be();
    h.flags              = r.read_u16_be();
    h.units_per_em       = r.read_u16_be();

    // created / modified are LONGDATETIME (int64, seconds since 1904-01-01).
    uint32_t ch = r.read_u32_be(), cl = r.read_u32_be();
    h.created = (int64_t(ch) << 32) | cl;
    uint32_t mh = r.read_u32_be(), ml = r.read_u32_be();
    h.modified = (int64_t(mh) << 32) | ml;

    h.x_min = r.read_i16_be();
    h.y_min = r.read_i16_be();
    h.x_max = r.read_i16_be();
    h.y_max = r.read_i16_be();

    h.mac_style           = r.read_u16_be();
    h.lowest_rec_ppem     = r.read_u16_be();
    h.font_direction_hint = r.read_i16_be();
    h.index_to_loc_format = r.read_i16_be();
    h.glyph_data_format   = r.read_i16_be();

    if (!r.ok()) return Result<HeadTable>::error(Status::TruncatedInput);

    if (h.magic_number != 0x5F0F3CF5u)
        return Result<HeadTable>::error(Status::MalformedTable);
    if (h.units_per_em < 16 || h.units_per_em > 16384)
        return Result<HeadTable>::error(Status::MalformedTable);
    if (h.index_to_loc_format != 0 && h.index_to_loc_format != 1)
        return Result<HeadTable>::error(Status::MalformedTable);

    return Result<HeadTable>::success(h);
}

Result<MaxpTable> parse_maxp(ByteReader& r) {
    MaxpTable m{};
    m.version    = r.read_u32_be();
    m.num_glyphs = r.read_u16_be();
    if (!r.ok()) return Result<MaxpTable>::error(Status::TruncatedInput);

    if (m.version == 0x00010000u) {
        m.max_points              = r.read_u16_be();
        m.max_contours            = r.read_u16_be();
        m.max_composite_points    = r.read_u16_be();
        m.max_composite_contours  = r.read_u16_be();
        m.max_zones               = r.read_u16_be();
        m.max_twilight_points     = r.read_u16_be();
        m.max_storage             = r.read_u16_be();
        m.max_function_defs       = r.read_u16_be();
        m.max_instruction_defs    = r.read_u16_be();
        m.max_stack_elements      = r.read_u16_be();
        m.max_size_of_instructions= r.read_u16_be();
        m.max_component_elements  = r.read_u16_be();
        m.max_component_depth     = r.read_u16_be();
        if (!r.ok()) return Result<MaxpTable>::error(Status::TruncatedInput);
    }

    return Result<MaxpTable>::success(m);
}

Result<HheaTable> parse_hhea(ByteReader& r) {
    HheaTable h{};
    h.major_version        = r.read_u16_be();
    h.minor_version        = r.read_u16_be();
    h.ascender             = r.read_i16_be();
    h.descender            = r.read_i16_be();
    h.line_gap             = r.read_i16_be();
    h.advance_width_max    = r.read_u16_be();
    h.min_left_side_bearing = r.read_i16_be();
    h.min_right_side_bearing= r.read_i16_be();
    h.x_max_extent         = r.read_i16_be();
    h.caret_slope_rise     = r.read_i16_be();
    h.caret_slope_run      = r.read_i16_be();
    h.caret_offset         = r.read_i16_be();
    r.skip(8);  // reserved
    h.metric_data_format   = r.read_i16_be();
    h.number_of_h_metrics  = r.read_u16_be();
    if (!r.ok()) return Result<HheaTable>::error(Status::TruncatedInput);
    return Result<HheaTable>::success(h);
}

Result<HmtxTable> parse_hmtx(ByteReader& r, uint16_t num_glyphs,
                               uint16_t number_of_h_metrics)
{
    HmtxTable hmtx;
    if (number_of_h_metrics > num_glyphs)
        return Result<HmtxTable>::error(Status::MalformedTable);

    hmtx.hmetrics.reserve(number_of_h_metrics);
    for (uint16_t i = 0; i < number_of_h_metrics; ++i) {
        GlyphHMetrics m;
        m.advance_width = r.read_u16_be();
        m.lsb           = r.read_i16_be();
        if (!r.ok()) return Result<HmtxTable>::error(Status::TruncatedInput);
        hmtx.hmetrics.push_back(m);
    }

    uint16_t lsb_count = num_glyphs - number_of_h_metrics;
    hmtx.left_side_bearings.reserve(lsb_count);
    for (uint16_t i = 0; i < lsb_count; ++i) {
        hmtx.left_side_bearings.push_back(r.read_i16_be());
        if (!r.ok()) return Result<HmtxTable>::error(Status::TruncatedInput);
    }

    return Result<HmtxTable>::success(std::move(hmtx));
}

Result<PostTable> parse_post(ByteReader& r) {
    PostTable p{};
    p.version             = Fixed16::from_raw(r.read_i32_be());
    p.italic_angle        = Fixed16::from_raw(r.read_i32_be());
    p.underline_position  = r.read_i16_be();
    p.underline_thickness = r.read_i16_be();
    p.is_fixed_pitch      = r.read_u32_be();
    p.min_mem_type42      = r.read_u32_be();
    p.max_mem_type42      = r.read_u32_be();
    p.min_mem_type1       = r.read_u32_be();
    p.max_mem_type1       = r.read_u32_be();
    if (!r.ok()) return Result<PostTable>::error(Status::TruncatedInput);

    // Version 2.0 carries an explicit glyph-name index plus a packed list of
    // Pascal strings for the names that are not in the standard Macintosh set.
    if (p.version.raw == 0x00020000) {
        p.num_names = r.read_u16_be();
        if (!r.ok()) return Result<PostTable>::error(Status::TruncatedInput);

        p.name_index.reserve(p.num_names);
        uint16_t max_custom = 0;
        for (uint16_t i = 0; i < p.num_names; ++i) {
            uint16_t idx = r.read_u16_be();
            if (!r.ok()) return Result<PostTable>::error(Status::TruncatedInput);
            p.name_index.push_back(idx);
            if (idx >= 258 && uint16_t(idx - 258 + 1) > max_custom)
                max_custom = uint16_t(idx - 258 + 1);
        }

        // The Pascal strings follow the index array, in storage order. There
        // are as many as remain in the table; a well-formed font supplies one
        // per distinct custom index but we read until the subtable is consumed.
        p.custom_names.reserve(max_custom);
        while (r.remaining() > 0) {
            uint8_t len = r.read_u8();
            if (!r.ok()) break;
            std::string s(len, '\0');
            if (len && !r.read_bytes(reinterpret_cast<uint8_t*>(&s[0]), len))
                break;
            p.custom_names.push_back(std::move(s));
        }
    }

    return Result<PostTable>::success(p);
}

// The standard Macintosh glyph ordering. Indices 0..257 in a version 2.0 post
// table that fall below 258 name the corresponding entry here.
static const char* const kMacGlyphNames[] = {
    ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl",
    "numbersign", "dollar", "percent", "ampersand", "quotesingle",
    "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen",
    "period", "slash", "zero", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "colon", "semicolon", "less",
    "equal", "greater", "question", "at",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
};
static constexpr uint16_t kMacGlyphNameCount =
    sizeof(kMacGlyphNames) / sizeof(kMacGlyphNames[0]);

// Built-in fallback names used when a font references more custom names than
// it actually stores in the post string list.
static const char* const kFallbackNames[8] = {
    "glyph0", "glyph1", "glyph2", "glyph3",
    "glyph4", "glyph5", "glyph6", "glyph7",
};

std::string post_glyph_name(const PostTable& post, uint16_t glyph_id) {
    if (post.name_index.empty() || glyph_id >= post.name_index.size())
        return std::string();

    uint16_t idx = post.name_index[glyph_id];
    if (idx < 258) {
        // A standard name from the Macintosh ordering.
        if (idx < kMacGlyphNameCount)
            return std::string(kMacGlyphNames[idx]);
        return "cid" + std::to_string(idx);
    }

    uint16_t custom = uint16_t(idx - 258);
    if (custom < post.custom_names.size())
        return post.custom_names[custom];

    // The font referenced a custom name it did not store; substitute a
    // built-in fallback keyed by how far past the stored list we are.
    size_t fb = size_t(custom) - post.custom_names.size();
    if (fb < sizeof(kFallbackNames) / sizeof(kFallbackNames[0]))
        return std::string(kFallbackNames[fb]);
    return std::string();
}

Result<Os2Table> parse_os2(ByteReader& r) {
    Os2Table o{};
    o.version           = r.read_u16_be();
    o.x_avg_char_width  = r.read_i16_be();
    o.us_weight_class   = r.read_u16_be();
    o.us_width_class    = r.read_u16_be();
    o.fs_type           = r.read_u16_be();
    o.y_subscript_x_size    = r.read_i16_be();
    o.y_subscript_y_size    = r.read_i16_be();
    o.y_subscript_x_offset  = r.read_i16_be();
    o.y_subscript_y_offset  = r.read_i16_be();
    o.y_superscript_x_size  = r.read_i16_be();
    o.y_superscript_y_size  = r.read_i16_be();
    o.y_superscript_x_offset= r.read_i16_be();
    o.y_superscript_y_offset= r.read_i16_be();
    o.y_strikeout_size     = r.read_i16_be();
    o.y_strikeout_position = r.read_i16_be();
    o.s_family_class       = r.read_i16_be();
    r.read_bytes(o.panose, 10);
    for (int i = 0; i < 4; ++i)
        o.ul_unicode_range[i] = r.read_u32_be();
    r.read_bytes(o.ach_vend_id, 4);
    o.fs_selection        = r.read_u16_be();
    o.us_first_char_index = r.read_u16_be();
    o.us_last_char_index  = r.read_u16_be();
    o.s_typo_ascender     = r.read_i16_be();
    o.s_typo_descender    = r.read_i16_be();
    o.s_typo_line_gap     = r.read_i16_be();
    o.us_win_ascent       = r.read_u16_be();
    o.us_win_descent      = r.read_u16_be();
    if (!r.ok()) return Result<Os2Table>::error(Status::TruncatedInput);
    return Result<Os2Table>::success(o);
}

GlyphHMetrics get_glyph_hmetrics(const HmtxTable& hmtx, uint16_t glyph_id) {
    if (!hmtx.hmetrics.empty() && glyph_id < hmtx.hmetrics.size())
        return hmtx.hmetrics[glyph_id];

    GlyphHMetrics m{};
    if (!hmtx.hmetrics.empty())
        m.advance_width = hmtx.hmetrics.back().advance_width;

    size_t lsb_idx = glyph_id - hmtx.hmetrics.size();
    if (lsb_idx < hmtx.left_side_bearings.size())
        m.lsb = hmtx.left_side_bearings[lsb_idx];
    return m;
}

} // namespace fontscope
