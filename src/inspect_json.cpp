#include "fontscope/inspect_json.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace fontscope {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if (c == '"')       { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n";  }
        else if (c == '\r') { out += "\\r";  }
        else if (c == '\t') { out += "\\t";  }
        else if (c < 0x20)  {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04X", c);
            out += buf;
        } else {
            out += char(c);
        }
    }
    return out;
}

void JsonWriter::key(const char* k) {
    comma_if_needed();
    buf_ += '"';
    buf_ += k;
    buf_ += "\":";
    sep_ = false;
}

void JsonWriter::value_string(const std::string& v) {
    comma_if_needed();
    buf_ += '"';
    buf_ += json_escape(v);
    buf_ += '"';
}

void JsonWriter::value_string(const char* v) {
    value_string(std::string(v ? v : ""));
}

void JsonWriter::value_int(int64_t v) {
    comma_if_needed();
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    buf_ += buf;
}

void JsonWriter::value_uint(uint64_t v) {
    comma_if_needed();
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
    buf_ += buf;
}

void JsonWriter::value_double(double v, int precision) {
    comma_if_needed();
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", precision, v);
    buf_ += buf;
}

void JsonWriter::value_bool(bool v) {
    comma_if_needed();
    buf_ += v ? "true" : "false";
}

void JsonWriter::value_null() {
    comma_if_needed();
    buf_ += "null";
}

std::string font_info_to_json(const FontFace& font) {
    JsonWriter w;
    w.begin_object();

    const char* empty = "";
    auto get_name = [&](uint16_t id) -> std::string {
        if (!font.has_name) return empty;
        const std::string* v = font.name.find(id);
        return v ? *v : empty;
    };

    w.kv_string("sfntVersion",  sfnt_version_name(font.sfnt.sfVersion));
    w.kv_uint("numTables",      font.sfnt.numTables);
    w.kv_string("family",       get_name(1));
    w.kv_string("subfamily",    get_name(2));
    w.kv_string("version",      get_name(5));
    w.kv_string("fullName",     get_name(4));
    w.kv_string("postScriptName", get_name(6));
    w.kv_uint("numGlyphs",      font.maxp.num_glyphs);
    w.kv_uint("unitsPerEm",     font.head.units_per_em);

    w.key("bbox");
    w.begin_object();
    w.kv_int("xMin", font.head.x_min);
    w.kv_int("yMin", font.head.y_min);
    w.kv_int("xMax", font.head.x_max);
    w.kv_int("yMax", font.head.y_max);
    w.end_object();

    w.key("hhea");
    w.begin_object();
    w.kv_int("ascender",  font.hhea.ascender);
    w.kv_int("descender", font.hhea.descender);
    w.kv_int("lineGap",   font.hhea.line_gap);
    w.end_object();

    if (font.has_os2) {
        w.key("os2");
        w.begin_object();
        w.kv_int("typoAscender",   font.os2.s_typo_ascender);
        w.kv_int("typoDescender",  font.os2.s_typo_descender);
        w.kv_int("typoLineGap",    font.os2.s_typo_line_gap);
        w.kv_uint("winAscent",     font.os2.us_win_ascent);
        w.kv_uint("winDescent",    font.os2.us_win_descent);
        w.kv_uint("weightClass",   font.os2.us_weight_class);
        w.kv_uint("widthClass",    font.os2.us_width_class);
        w.end_object();
    }

    w.kv_bool("hasColr", font.has_colr);
    w.kv_bool("hasFvar", font.has_fvar);
    if (font.has_colr)  w.kv_uint("numColorGlyphs", font.colr.glyphs.size());
    if (font.has_fvar)  w.kv_uint("numAxes",        font.fvar.axes.size());

    w.kv_double("italicAngle", font.post.italic_angle.to_f64(), 3);
    w.kv_bool("isFixedPitch",  font.post.is_fixed_pitch != 0);

    w.end_object();
    return w.str();
}

std::string tables_to_json(const FontFace& font) {
    JsonWriter w;
    w.begin_array();

    for (const auto& t : font.sfnt.tables) {
        char tag[5];
        tag[0] = char((t.tag.value >> 24) & 0xFF);
        tag[1] = char((t.tag.value >> 16) & 0xFF);
        tag[2] = char((t.tag.value >>  8) & 0xFF);
        tag[3] = char((t.tag.value      ) & 0xFF);
        tag[4] = '\0';

        bool ok = verify_table_checksum(t, font.raw_data.data(), font.raw_data.size());

        w.begin_object();
        w.kv_string("tag",       tag);
        w.kv_uint("offset",      t.offset);
        w.kv_uint("length",      t.length);
        w.kv_uint("checksum",    t.checksum);
        w.kv_bool("checksumOk",  ok);
        w.end_object();
    }

    w.end_array();
    return w.str();
}

std::string validation_report_to_json(const ValidationReport& report) {
    JsonWriter w;
    w.begin_object();
    w.kv_int("errors",   report.error_count);
    w.kv_int("warnings", report.warning_count);

    w.key("issues");
    w.begin_array();
    for (const auto& issue : report.issues) {
        const char* sev;
        switch (issue.severity) {
        case IssueSeverity::Error:   sev = "error";   break;
        case IssueSeverity::Warning: sev = "warning"; break;
        default:                     sev = "info";    break;
        }
        w.begin_object();
        w.kv_string("severity", sev);
        w.kv_string("table",    issue.table);
        w.kv_string("message",  issue.message);
        w.end_object();
    }
    w.end_array();

    w.end_object();
    return w.str();
}

std::string glyph_to_json(const RawGlyph& g, uint16_t glyph_id) {
    JsonWriter w;
    w.begin_object();
    w.kv_uint("glyphId", glyph_id);
    w.kv_string("type", g.is_composite() ? "composite"
                       : g.is_empty()     ? "empty"
                       :                    "simple");
    w.kv_int("xMin", g.x_min);
    w.kv_int("yMin", g.y_min);
    w.kv_int("xMax", g.x_max);
    w.kv_int("yMax", g.y_max);

    if (!g.is_composite() && !g.is_empty()) {
        w.kv_uint("numContours", g.end_pts_of_contours.size());
        w.kv_uint("numPoints",   g.points.size());
        w.kv_uint("instructionBytes", g.instructions.size());

        w.key("contours");
        w.begin_array();
        uint16_t pt = 0;
        for (size_t c = 0; c < g.end_pts_of_contours.size(); ++c) {
            uint16_t end = g.end_pts_of_contours[c];
            w.begin_object();
            w.key("points");
            w.begin_array();
            for (; pt <= end && pt < g.points.size(); ++pt) {
                const FPoint& p = g.points[pt];
                w.begin_object();
                w.kv_int("x", p.x);
                w.kv_int("y", p.y);
                w.kv_bool("onCurve", p.on_curve);
                w.end_object();
            }
            w.end_array();
            w.end_object();
        }
        w.end_array();
    } else if (g.is_composite()) {
        w.key("components");
        w.begin_array();
        for (const auto& comp : g.components) {
            w.begin_object();
            w.kv_uint("glyphIndex", comp.glyph_index);
            w.kv_int("dx",          comp.arg1);
            w.kv_int("dy",          comp.arg2);
            w.end_object();
        }
        w.end_array();
    }

    w.end_object();
    return w.str();
}

std::string cmap_to_json(const FontFace& font, uint32_t cp_start, uint32_t cp_end) {
    JsonWriter w;
    w.begin_object();

    w.key("subtables");
    w.begin_array();
    for (const auto& st : font.cmap.subtables) {
        w.begin_object();
        w.kv_uint("platformId", st.platform_id);
        w.kv_uint("encodingId", st.encoding_id);
        w.kv_uint("format",     st.format);
        w.end_object();
    }
    w.end_array();

    w.key("mappings");
    w.begin_array();
    for (uint32_t cp = cp_start; cp <= cp_end; ++cp) {
        uint16_t gid = font.cmap.lookup(cp);
        if (gid == 0) continue;
        w.begin_object();
        w.kv_uint("codepoint", cp);
        w.kv_uint("glyphId",   gid);
        w.end_object();
    }
    w.end_array();

    w.end_object();
    return w.str();
}

std::string name_table_to_json(const FontFace& font) {
    JsonWriter w;
    w.begin_array();
    if (font.has_name) {
        for (const auto& nr : font.name.records) {
            w.begin_object();
            w.kv_uint("platformId", nr.platform_id);
            w.kv_uint("encodingId", nr.encoding_id);
            w.kv_uint("languageId", nr.language_id);
            w.kv_uint("nameId",     nr.name_id);
            w.kv_string("value",    nr.value);
            w.end_object();
        }
    }
    w.end_array();
    return w.str();
}

std::string fvar_to_json(const FontFace& font) {
    JsonWriter w;
    w.begin_object();

    w.key("axes");
    w.begin_array();
    if (font.has_fvar) {
        for (const auto& ax : font.fvar.axes) {
            char tag[5];
            tag[0] = char((ax.axis_tag.value >> 24) & 0xFF);
            tag[1] = char((ax.axis_tag.value >> 16) & 0xFF);
            tag[2] = char((ax.axis_tag.value >>  8) & 0xFF);
            tag[3] = char((ax.axis_tag.value      ) & 0xFF);
            tag[4] = '\0';

            w.begin_object();
            w.kv_string("tag",     tag);
            w.kv_double("min",     ax.min_value.to_f64(),     3);
            w.kv_double("default", ax.default_value.to_f64(), 3);
            w.kv_double("max",     ax.max_value.to_f64(),     3);
            w.kv_uint("nameId",    ax.axis_name_id);
            w.end_object();
        }
    }
    w.end_array();

    w.key("instances");
    w.begin_array();
    if (font.has_fvar) {
        for (const auto& ni : font.fvar.named_instances) {
            w.begin_object();
            w.kv_uint("nameId", ni.subfamily_name_id);
            w.key("coordinates");
            w.begin_array();
            for (const auto& c : ni.coordinates) w.value_double(c.to_f64(), 3);
            w.end_array();
            w.end_object();
        }
    }
    w.end_array();

    w.end_object();
    return w.str();
}

std::string outline_summary_to_json(const OutlineSummary& s) {
    JsonWriter w;
    w.begin_object();
    w.kv_uint("totalGlyphs",           s.total_glyphs);
    w.kv_uint("simpleGlyphs",          s.simple_glyphs);
    w.kv_uint("compositeGlyphs",       s.composite_glyphs);
    w.kv_uint("emptyGlyphs",           s.empty_glyphs);
    w.kv_uint("glyphsWithInstructions",s.glyphs_with_instructions);
    w.kv_uint("totalPoints",           s.total_points);
    w.kv_uint("totalContours",         s.total_contours);
    w.kv_uint("onCurvePoints",         s.total_on_curve);
    w.kv_uint("offCurvePoints",        s.total_off_curve);
    w.kv_uint("totalInstructionBytes", s.total_instruction_bytes);
    w.kv_uint("maxPoints",             s.max_points);
    w.kv_uint("maxContours",           s.max_contours);
    w.kv_double("avgPointsPerGlyph",   s.avg_points_per_glyph,   1);
    w.kv_double("avgContoursPerGlyph", s.avg_contours_per_glyph, 1);
    w.end_object();
    return w.str();
}

} // namespace fontscope
