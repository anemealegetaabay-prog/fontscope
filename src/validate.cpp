#include "fontscope/validate.h"
#include <cstdio>
#include <cstring>

namespace fontscope {

static const char* severity_str(IssueSeverity s) {
    switch (s) {
    case IssueSeverity::Info:    return "info";
    case IssueSeverity::Warning: return "warn";
    case IssueSeverity::Error:   return "error";
    default: return "?";
    }
}

ValidationReport validate_font(const FontFace& font) {
    ValidationReport report;

    // head: magic
    if (font.head.magic_number != 0x5F0F3CF5u)
        report.add(IssueSeverity::Error, "head",
                   "magic_number is not 0x5F0F3CF5");

    // head: units per em range per spec
    if (font.head.units_per_em < 16 || font.head.units_per_em > 16384)
        report.add(IssueSeverity::Error, "head",
                   "unitsPerEm out of range [16, 16384]");

    // head: bbox self-consistency
    if (font.head.x_min > font.head.x_max || font.head.y_min > font.head.y_max)
        report.add(IssueSeverity::Warning, "head",
                   "bounding box min > max");

    // head: index_to_loc_format
    if (font.head.index_to_loc_format != 0 && font.head.index_to_loc_format != 1)
        report.add(IssueSeverity::Error, "head",
                   "indexToLocFormat must be 0 or 1");

    // maxp: num_glyphs > 0
    if (font.maxp.num_glyphs == 0)
        report.add(IssueSeverity::Error, "maxp", "numGlyphs is 0");

    // hhea: numberOfHMetrics <= numGlyphs
    if (font.hhea.number_of_h_metrics > font.maxp.num_glyphs)
        report.add(IssueSeverity::Error, "hhea",
                   "numberOfHMetrics > maxp.numGlyphs");

    // loca: correct entry count
    if (!font.loca.offsets.empty()) {
        size_t expected = size_t(font.maxp.num_glyphs) + 1;
        if (font.loca.offsets.size() != expected)
            report.add(IssueSeverity::Error, "loca",
                       "loca entry count does not match numGlyphs+1");
    }

    // table checksums
    for (const auto& t : font.sfnt.tables) {
        bool ok = verify_table_checksum(t, font.raw_data.data(),
                                        font.raw_data.size());
        if (!ok) {
            char tag[5];
            tag[0] = char((t.tag.value >> 24) & 0xFF);
            tag[1] = char((t.tag.value >> 16) & 0xFF);
            tag[2] = char((t.tag.value >>  8) & 0xFF);
            tag[3] = char((t.tag.value      ) & 0xFF);
            tag[4] = '\0';
            report.add(IssueSeverity::Warning, tag, "checksum mismatch");
        }
    }

    // table offsets within file bounds
    for (const auto& t : font.sfnt.tables) {
        if (uint64_t(t.offset) + t.length > font.raw_data.size()) {
            char tag[5];
            tag[0] = char((t.tag.value >> 24) & 0xFF);
            tag[1] = char((t.tag.value >> 16) & 0xFF);
            tag[2] = char((t.tag.value >>  8) & 0xFF);
            tag[3] = char((t.tag.value      ) & 0xFF);
            tag[4] = '\0';
            report.add(IssueSeverity::Error, tag,
                       "table extends past end of file");
        }
    }

    // OS/2 consistency
    if (font.has_os2) {
        if (font.os2.us_weight_class < 1 || font.os2.us_weight_class > 1000)
            report.add(IssueSeverity::Warning, "OS/2",
                       "usWeightClass outside [1, 1000]");
        if (font.os2.us_width_class < 1 || font.os2.us_width_class > 9)
            report.add(IssueSeverity::Warning, "OS/2",
                       "usWidthClass outside [1, 9]");
    }

    // COLR layer index bounds
    if (font.has_colr) {
        for (const auto& g : font.colr.glyphs) {
            size_t end = size_t(g.first_layer_index) + g.num_layers;
            if (end > font.colr.layers.size())
                report.add(IssueSeverity::Error, "COLR",
                           "color glyph layer range out of bounds");
        }
    }

    // fvar axis consistency
    if (font.has_fvar) {
        for (const auto& ax : font.fvar.axes) {
            if (ax.min_value > ax.default_value ||
                ax.default_value > ax.max_value)
                report.add(IssueSeverity::Warning, "fvar",
                           "axis default out of [min, max] range");
        }
    }

    // name: check family name present
    if (font.has_name) {
        if (!font.name.find(uint16_t(NameId::FamilyName)))
            report.add(IssueSeverity::Warning, "name",
                       "no family name record (nameID=1)");
    }

    if (report.issues.empty())
        report.add(IssueSeverity::Info, "*", "no issues found");

    return report;
}

void print_validation_report(const ValidationReport& report) {
    for (const auto& issue : report.issues)
        printf("[%s] %-6s %s\n",
               severity_str(issue.severity),
               issue.table.c_str(),
               issue.message.c_str());

    printf("\n%d error(s), %d warning(s)\n",
           report.error_count, report.warning_count);
}

} // namespace fontscope
