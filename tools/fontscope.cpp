#include "fontscope/inspect.h"
#include "fontscope/validate.h"
#include "fontscope/metrics.h"
#include "fontscope/kern.h"
#include "fontscope/hint_vm.h"
#include "fontscope/bitmap.h"
#include "fontscope/deltas.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>

using namespace fontscope;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static void usage(const char* prog) {
    printf("Usage: %s <command> <font.ttf> [args]\n\n", prog);
    printf("Commands:\n");
    printf("  inspect   <font>              Show font metadata\n");
    printf("  tables    <font>              List all font tables\n");
    printf("  validate  <font>              Validate table structure\n");
    printf("  dump-glyph <font> <id|U+cp>  Dump glyph outline\n");
    printf("  cmap      <font>              List cmap subtables and mappings\n");
    printf("  name      <font>              Print all name records\n");
    printf("  metrics   <font>              Print font metrics\n");
    printf("  advances  <font> [from] [to]  Print per-glyph advance widths\n");
    printf("  outline-stats <font>          Count points, contours\n");
    printf("  kern-lookup <font> <L> <R>    Kerning value for glyph pair\n");
    printf("  color-glyphs <font>           List COLR color glyphs\n");
    printf("  var-axes  <font>              List variation axes\n");
}

static int cmd_inspect(const FontFace& font) {
    print_font_info(font);
    return 0;
}

static int cmd_tables(const FontFace& font) {
    print_tables(font);
    return 0;
}

static int cmd_validate(const FontFace& font) {
    auto report = validate_font(font);
    print_validation_report(report);
    return report.ok() ? 0 : 1;
}

static int cmd_dump_glyph(const FontFace& font, const char* id_arg) {
    uint16_t glyph_id = 0;

    if (id_arg[0] == 'U' && id_arg[1] == '+') {
        uint32_t cp = uint32_t(strtoul(id_arg + 2, nullptr, 16));
        glyph_id = codepoint_to_glyph(font, cp);
        if (glyph_id == 0) {
            printf("No glyph found for U+%04X\n", cp);
            return 1;
        }
        printf("U+%04X → glyph %u\n", cp, glyph_id);
    } else {
        glyph_id = uint16_t(atoi(id_arg));
    }

    auto gr = load_glyph(font, glyph_id);
    if (!gr.ok()) {
        printf("Error loading glyph %u: %s\n", glyph_id, status_string(gr.status));
        return 1;
    }

    const RawGlyph& g = gr.value;
    printf("Glyph %u:\n", glyph_id);
    printf("  Type: %s\n", g.is_composite() ? "composite"
                         : g.is_empty()     ? "empty"
                         :                    "simple");
    printf("  Bbox: (%d, %d)–(%d, %d)\n", g.x_min, g.y_min, g.x_max, g.y_max);

    if (!g.is_composite() && !g.is_empty()) {
        printf("  Contours: %zu\n", g.end_pts_of_contours.size());
        printf("  Points:   %zu\n", g.points.size());
        printf("  Instructions: %zu bytes\n", g.instructions.size());

        uint16_t pt = 0;
        for (size_t c = 0; c < g.end_pts_of_contours.size(); ++c) {
            uint16_t end = g.end_pts_of_contours[c];
            printf("  Contour %zu (%u pts):\n", c, uint16_t(end - pt + 1));
            for (; pt <= end && pt < g.points.size(); ++pt) {
                const FPoint& p = g.points[pt];
                printf("    [%u] %d %d %s\n", pt, p.x, p.y,
                       p.on_curve ? "on" : "off");
            }
        }
    } else if (g.is_composite()) {
        for (size_t i = 0; i < g.components.size(); ++i) {
            const auto& comp = g.components[i];
            printf("  Component %zu: glyph=%u dx=%d dy=%d\n",
                   i, comp.glyph_index, comp.arg1, comp.arg2);
        }
    }

    // Show advance metrics too.
    GlyphHMetrics hm = get_glyph_hmetrics(font.hmtx, glyph_id);
    printf("  AdvanceWidth: %u  LSB: %d\n", hm.advance_width, hm.lsb);
    return 0;
}

static int cmd_cmap(const FontFace& font) {
    printf("cmap subtables (%zu):\n", font.cmap.subtables.size());
    for (const auto& st : font.cmap.subtables) {
        printf("  platform=%u encoding=%u format=%u\n",
               st.platform_id, st.encoding_id, st.format);
    }

    printf("\nSample mappings (U+0020–U+00FF):\n");
    for (uint32_t cp = 0x20; cp <= 0xFF; cp += 8) {
        for (uint32_t c2 = cp; c2 < cp + 8 && c2 <= 0xFF; ++c2) {
            uint16_t gid = font.cmap.lookup(c2);
            if (gid) printf("  U+%04X → %u\n", c2, gid);
        }
    }
    return 0;
}

static int cmd_name(const FontFace& font) {
    if (!font.has_name) {
        printf("No name table found.\n");
        return 1;
    }
    struct { uint16_t id; const char* label; } names[] = {
        {0,  "Copyright"},
        {1,  "Family"},
        {2,  "Subfamily"},
        {3,  "Unique ID"},
        {4,  "Full name"},
        {5,  "Version"},
        {6,  "PostScript name"},
        {8,  "Manufacturer"},
        {9,  "Designer"},
        {10, "Description"},
        {11, "Vendor URL"},
        {12, "Designer URL"},
        {13, "License"},
        {14, "License URL"},
        {16, "Typographic family"},
        {17, "Typographic subfamily"},
        {19, "Sample text"},
    };
    for (const auto& n : names) {
        const std::string* v = font.name.find(n.id);
        if (v) printf("  [%2u] %-22s %s\n", n.id, n.label, v->c_str());
    }
    return 0;
}

static int cmd_metrics(const FontFace& font) {
    print_metrics_summary(font);
    return 0;
}

static int cmd_advances(const FontFace& font, const char* from_arg, const char* to_arg) {
    uint16_t from = from_arg ? uint16_t(atoi(from_arg)) : 0u;
    uint16_t to   = to_arg   ? uint16_t(atoi(to_arg))
                             : std::min<uint16_t>(font.maxp.num_glyphs, 100u);
    print_glyph_advances(font, from, to);
    return 0;
}

static int cmd_outline_stats(const FontFace& font) {
    auto s = collect_outline_stats(font);
    print_outline_stats(s);
    return 0;
}

static int cmd_kern_lookup(const FontFace& font, const char* left_arg, const char* right_arg) {
    const TableRecord* kern_rec = find_table(font.sfnt, tags::KERN());
    if (!kern_rec) { printf("No kern table.\n"); return 1; }

    ByteReader r(font.raw_data.data(), font.raw_data.size());
    ByteReader kr = r.sub_reader(kern_rec->offset, kern_rec->length);
    auto res = parse_kern(kr);
    if (!res.ok()) { printf("Error parsing kern table.\n"); return 1; }

    uint16_t L = uint16_t(atoi(left_arg));
    uint16_t R = uint16_t(atoi(right_arg));
    int16_t adj = res.value.lookup(L, R);
    printf("kern(%u, %u) = %d FUnits\n", L, R, adj);
    return 0;
}

static int cmd_color_glyphs(const FontFace& font) {
    if (!font.has_colr) { printf("No COLR table.\n"); return 1; }
    printf("COLR v%u: %zu color glyphs, %zu layers\n",
           font.colr.version, font.colr.glyphs.size(), font.colr.layers.size());
    for (const auto& g : font.colr.glyphs) {
        printf("  glyph=%u first_layer=%u num_layers=%u\n",
               g.glyph_id, g.first_layer_index, g.num_layers);
    }
    return 0;
}

static int cmd_var_axes(const FontFace& font) {
    if (!font.has_fvar) { printf("No fvar table.\n"); return 1; }
    printf("Variation axes (%zu):\n", font.fvar.axes.size());
    for (const auto& ax : font.fvar.axes) {
        char tag[5];
        tag[0] = char((ax.axis_tag.value >> 24) & 0xFF);
        tag[1] = char((ax.axis_tag.value >> 16) & 0xFF);
        tag[2] = char((ax.axis_tag.value >>  8) & 0xFF);
        tag[3] = char((ax.axis_tag.value      ) & 0xFF);
        tag[4] = '\0';
        printf("  '%s': min=%.3f default=%.3f max=%.3f (nameID=%u)\n",
               tag,
               ax.min_value.to_f64(),
               ax.default_value.to_f64(),
               ax.max_value.to_f64(),
               ax.axis_name_id);
    }
    if (!font.fvar.named_instances.empty()) {
        printf("Named instances (%zu):\n", font.fvar.named_instances.size());
        for (const auto& ni : font.fvar.named_instances) {
            printf("  nameID=%u:", ni.subfamily_name_id);
            for (const auto& c : ni.coordinates)
                printf(" %.3f", c.to_f64());
            printf("\n");
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char* cmd = argv[1];
    const char* font_path = argv[2];

    auto buf = read_file(font_path);
    if (buf.empty()) {
        fprintf(stderr, "Cannot read '%s'\n", font_path);
        return 1;
    }

    auto res = load_font(buf.data(), buf.size());
    if (!res.ok()) {
        fprintf(stderr, "Error loading font: %s\n", status_string(res.status));
        return 1;
    }
    const FontFace& font = res.value;

    if      (!strcmp(cmd, "inspect"))      return cmd_inspect(font);
    else if (!strcmp(cmd, "tables"))       return cmd_tables(font);
    else if (!strcmp(cmd, "validate"))     return cmd_validate(font);
    else if (!strcmp(cmd, "cmap"))         return cmd_cmap(font);
    else if (!strcmp(cmd, "name"))         return cmd_name(font);
    else if (!strcmp(cmd, "metrics"))      return cmd_metrics(font);
    else if (!strcmp(cmd, "outline-stats"))return cmd_outline_stats(font);
    else if (!strcmp(cmd, "color-glyphs")) return cmd_color_glyphs(font);
    else if (!strcmp(cmd, "var-axes"))     return cmd_var_axes(font);
    else if (!strcmp(cmd, "dump-glyph")) {
        if (argc < 4) { fprintf(stderr, "dump-glyph requires glyph id/codepoint\n"); return 1; }
        return cmd_dump_glyph(font, argv[3]);
    }
    else if (!strcmp(cmd, "advances")) {
        return cmd_advances(font, argc > 3 ? argv[3] : nullptr,
                                  argc > 4 ? argv[4] : nullptr);
    }
    else if (!strcmp(cmd, "kern-lookup")) {
        if (argc < 5) { fprintf(stderr, "kern-lookup requires two glyph IDs\n"); return 1; }
        return cmd_kern_lookup(font, argv[3], argv[4]);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }
}
