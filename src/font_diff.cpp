#include "fontscope/font_diff.h"
#include "fontscope/sfnt.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace fontscope {

static std::string itos(int64_t v) {
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

static FontDiffEntry entry(DiffKind k, const char* loc,
                            const std::string& a, const std::string& b) {
    return {k, loc, a, b};
}

bool compare_head(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out)
{
    bool changed = false;
    if (a.head.units_per_em != b.head.units_per_em) {
        out.push_back(entry(DiffKind::UpmChanged, "head.unitsPerEm",
            itos(a.head.units_per_em), itos(b.head.units_per_em)));
        changed = true;
    }
    if (a.head.index_to_loc_format != b.head.index_to_loc_format) {
        out.push_back(entry(DiffKind::MetricMismatch, "head.indexToLocFormat",
            itos(a.head.index_to_loc_format), itos(b.head.index_to_loc_format)));
        changed = true;
    }
    if (a.head.x_min != b.head.x_min || a.head.y_min != b.head.y_min ||
        a.head.x_max != b.head.x_max || a.head.y_max != b.head.y_max) {
        out.push_back(entry(DiffKind::MetricMismatch, "head.bbox",
            std::string("(") + itos(a.head.x_min) + "," + itos(a.head.y_min) +
            ")-(" + itos(a.head.x_max) + "," + itos(a.head.y_max) + ")",
            std::string("(") + itos(b.head.x_min) + "," + itos(b.head.y_min) +
            ")-(" + itos(b.head.x_max) + "," + itos(b.head.y_max) + ")"));
        changed = true;
    }
    if (a.head.mac_style != b.head.mac_style) {
        out.push_back(entry(DiffKind::MetricMismatch, "head.macStyle",
            itos(a.head.mac_style), itos(b.head.mac_style)));
        changed = true;
    }
    return !changed;
}

bool compare_hhea(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out)
{
    bool changed = false;
    if (a.hhea.ascender != b.hhea.ascender) {
        out.push_back(entry(DiffKind::MetricMismatch, "hhea.ascender",
            itos(a.hhea.ascender), itos(b.hhea.ascender)));
        changed = true;
    }
    if (a.hhea.descender != b.hhea.descender) {
        out.push_back(entry(DiffKind::MetricMismatch, "hhea.descender",
            itos(a.hhea.descender), itos(b.hhea.descender)));
        changed = true;
    }
    if (a.hhea.line_gap != b.hhea.line_gap) {
        out.push_back(entry(DiffKind::MetricMismatch, "hhea.lineGap",
            itos(a.hhea.line_gap), itos(b.hhea.line_gap)));
        changed = true;
    }
    if (a.hhea.advance_width_max != b.hhea.advance_width_max) {
        out.push_back(entry(DiffKind::MetricMismatch, "hhea.advanceWidthMax",
            itos(a.hhea.advance_width_max), itos(b.hhea.advance_width_max)));
        changed = true;
    }
    if (a.hhea.number_of_h_metrics != b.hhea.number_of_h_metrics) {
        out.push_back(entry(DiffKind::MetricMismatch, "hhea.numberOfHMetrics",
            itos(a.hhea.number_of_h_metrics), itos(b.hhea.number_of_h_metrics)));
        changed = true;
    }
    return !changed;
}

bool compare_maxp(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out)
{
    bool changed = false;
    if (a.maxp.num_glyphs != b.maxp.num_glyphs) {
        out.push_back(entry(DiffKind::GlyphCountChanged, "maxp.numGlyphs",
            itos(a.maxp.num_glyphs), itos(b.maxp.num_glyphs)));
        changed = true;
    }
    if (a.maxp.max_points != b.maxp.max_points) {
        out.push_back(entry(DiffKind::MetricMismatch, "maxp.maxPoints",
            itos(a.maxp.max_points), itos(b.maxp.max_points)));
        changed = true;
    }
    if (a.maxp.max_contours != b.maxp.max_contours) {
        out.push_back(entry(DiffKind::MetricMismatch, "maxp.maxContours",
            itos(a.maxp.max_contours), itos(b.maxp.max_contours)));
        changed = true;
    }
    if (a.maxp.max_composite_points != b.maxp.max_composite_points) {
        out.push_back(entry(DiffKind::MetricMismatch, "maxp.maxCompositePoints",
            itos(a.maxp.max_composite_points),
            itos(b.maxp.max_composite_points)));
        changed = true;
    }
    if (a.maxp.max_zones != b.maxp.max_zones) {
        out.push_back(entry(DiffKind::MetricMismatch, "maxp.maxZones",
            itos(a.maxp.max_zones), itos(b.maxp.max_zones)));
        changed = true;
    }
    return !changed;
}

bool compare_name(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out)
{
    bool changed = false;
    if (!a.has_name || !b.has_name) {
        if (a.has_name != b.has_name) {
            out.push_back(entry(a.has_name ? DiffKind::TableMissing
                                           : DiffKind::TableAdded,
                                "table:name", "", ""));
            changed = true;
        }
        return !changed;
    }

    // Compare well-known name IDs.
    for (uint16_t id = 0; id <= 20; ++id) {
        const std::string* va = a.name.find(id);
        const std::string* vb = b.name.find(id);
        bool has_a = va != nullptr;
        bool has_b = vb != nullptr;
        if (has_a != has_b || (has_a && *va != *vb)) {
            char loc[32];
            snprintf(loc, sizeof(loc), "name:%u", id);
            out.push_back(entry(DiffKind::NameChanged, loc,
                has_a ? *va : "(absent)",
                has_b ? *vb : "(absent)"));
            changed = true;
        }
    }
    return !changed;
}

bool compare_fvar(const FontFace& a, const FontFace& b,
                  std::vector<FontDiffEntry>& out)
{
    bool changed = false;
    if (a.has_fvar != b.has_fvar) {
        out.push_back(entry(a.has_fvar ? DiffKind::TableMissing
                                       : DiffKind::TableAdded,
                            "table:fvar", "", ""));
        return true;
    }
    if (!a.has_fvar) return true;  // neither has fvar

    if (a.fvar.axes.size() != b.fvar.axes.size()) {
        out.push_back(entry(DiffKind::AxisChanged, "fvar.numAxes",
            itos(a.fvar.axes.size()), itos(b.fvar.axes.size())));
        return false;
    }

    for (size_t i = 0; i < a.fvar.axes.size(); ++i) {
        const auto& ax = a.fvar.axes[i];
        const auto& bx = b.fvar.axes[i];
        if (ax.axis_tag.value != bx.axis_tag.value ||
            ax.min_value.raw != bx.min_value.raw ||
            ax.default_value.raw != bx.default_value.raw ||
            ax.max_value.raw != bx.max_value.raw)
        {
            char loc[32];
            snprintf(loc, sizeof(loc), "fvar.axis[%zu]", i);
            out.push_back(entry(DiffKind::AxisChanged, loc,
                "(min/def/max changed)", ""));
            changed = true;
        }
    }
    return !changed;
}

bool compare_table_checksums(const FontFace& a, const FontFace& b,
                              std::vector<FontDiffEntry>& out)
{
    bool any_changed = false;
    for (const auto& ta : a.sfnt.tables) {
        char tag[5];
        tag[0] = char((ta.tag.value >> 24) & 0xFF);
        tag[1] = char((ta.tag.value >> 16) & 0xFF);
        tag[2] = char((ta.tag.value >>  8) & 0xFF);
        tag[3] = char((ta.tag.value      ) & 0xFF);
        tag[4] = '\0';

        const TableRecord* tb = find_table(b.sfnt, ta.tag);
        if (!tb) {
            std::string loc = std::string("table:") + tag;
            out.push_back(entry(DiffKind::TableMissing, loc.c_str(), "", ""));
            any_changed = true;
            continue;
        }

        if (ta.checksum != tb->checksum || ta.length != tb->length) {
            std::string loc = std::string("table:") + tag;
            char av[32], bv[32];
            snprintf(av, sizeof(av), "len=%u csum=0x%08X", ta.length, ta.checksum);
            snprintf(bv, sizeof(bv), "len=%u csum=0x%08X", tb->length, tb->checksum);
            out.push_back(entry(DiffKind::TableChanged, loc.c_str(), av, bv));
            any_changed = true;
        }
    }

    for (const auto& tb : b.sfnt.tables) {
        if (!find_table(a.sfnt, tb.tag)) {
            char tag[5];
            tag[0] = char((tb.tag.value >> 24) & 0xFF);
            tag[1] = char((tb.tag.value >> 16) & 0xFF);
            tag[2] = char((tb.tag.value >>  8) & 0xFF);
            tag[3] = char((tb.tag.value      ) & 0xFF);
            tag[4] = '\0';
            std::string loc = std::string("table:") + tag;
            out.push_back(entry(DiffKind::TableAdded, loc.c_str(), "", ""));
            any_changed = true;
        }
    }

    return !any_changed;
}

FontDiff diff_fonts(const FontFace& a, const FontFace& b) {
    FontDiff diff{};
    diff.identical = true;

    auto run = [&](bool r) { if (!r) diff.identical = false; };
    run(compare_head(a, b, diff.entries));
    run(compare_hhea(a, b, diff.entries));
    run(compare_maxp(a, b, diff.entries));
    run(compare_name(a, b, diff.entries));
    run(compare_fvar(a, b, diff.entries));
    run(compare_table_checksums(a, b, diff.entries));

    for (const auto& e : diff.entries) {
        switch (e.kind) {
        case DiffKind::TableMissing:
        case DiffKind::TableAdded:
        case DiffKind::TableChanged:
        case DiffKind::MetricMismatch:
        case DiffKind::GlyphCountChanged:
        case DiffKind::UpmChanged:
        case DiffKind::ChecksumError:
            ++diff.n_errors;
            break;
        case DiffKind::NameChanged:
        case DiffKind::AxisChanged:
            ++diff.n_warnings;
            break;
        default:
            ++diff.n_info;
            break;
        }
    }

    return diff;
}

bool fonts_identical(const FontFace& a, const FontFace& b) {
    FontDiff d = diff_fonts(a, b);
    return d.identical && d.entries.empty();
}

void print_font_diff(const FontDiff& diff) {
    if (diff.entries.empty()) {
        printf("Fonts are identical.\n");
        return;
    }
    printf("Font diff: %d error(s), %d warning(s), %d info\n",
           diff.n_errors, diff.n_warnings, diff.n_info);
    for (const auto& e : diff.entries) {
        const char* kind;
        switch (e.kind) {
        case DiffKind::TableMissing:        kind = "MISSING_TABLE"; break;
        case DiffKind::TableAdded:          kind = "ADDED_TABLE";   break;
        case DiffKind::TableChanged:        kind = "TABLE_CHANGED"; break;
        case DiffKind::MetricMismatch:      kind = "METRIC";        break;
        case DiffKind::GlyphCountChanged:   kind = "GLYPH_COUNT";   break;
        case DiffKind::NameChanged:         kind = "NAME";          break;
        case DiffKind::AxisChanged:         kind = "AXIS";          break;
        case DiffKind::ChecksumError:       kind = "CHECKSUM";      break;
        case DiffKind::UpmChanged:          kind = "UPM";           break;
        default:                            kind = "INFO";           break;
        }
        printf("  [%s] %s", kind, e.location.c_str());
        if (!e.value_a.empty() || !e.value_b.empty())
            printf(": %s → %s", e.value_a.c_str(), e.value_b.c_str());
        printf("\n");
    }
}

} // namespace fontscope
