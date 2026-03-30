#include "fontscope/inspect.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace fontscope {

Result<FontFace> load_font(const uint8_t* data, size_t size) {
    if (!data || size < 12)
        return Result<FontFace>::error(Status::EmptyInput);

    FontFace f;
    f.raw_data.assign(data, data + size);

    ByteReader r(f.raw_data.data(), f.raw_data.size());

    auto sfnt_res = parse_sfnt_header(r);
    if (!sfnt_res.ok()) return Result<FontFace>::error(sfnt_res.status);
    f.sfnt = std::move(sfnt_res.value);

    // head
    const TableRecord* head_rec = find_table(f.sfnt, tags::HEAD());
    if (!head_rec) return Result<FontFace>::error(Status::TableNotFound);
    {
        ByteReader tr = r.sub_reader(head_rec->offset, head_rec->length);
        auto res = parse_head(tr);
        if (!res.ok()) return Result<FontFace>::error(res.status);
        f.head = res.value;
    }

    // maxp
    const TableRecord* maxp_rec = find_table(f.sfnt, tags::MAXP());
    if (!maxp_rec) return Result<FontFace>::error(Status::TableNotFound);
    {
        ByteReader tr = r.sub_reader(maxp_rec->offset, maxp_rec->length);
        auto res = parse_maxp(tr);
        if (!res.ok()) return Result<FontFace>::error(res.status);
        f.maxp = res.value;
    }

    // hhea
    const TableRecord* hhea_rec = find_table(f.sfnt, tags::HHEA());
    if (hhea_rec) {
        ByteReader tr = r.sub_reader(hhea_rec->offset, hhea_rec->length);
        auto res = parse_hhea(tr);
        if (res.ok()) f.hhea = res.value;
    }

    // hmtx
    const TableRecord* hmtx_rec = find_table(f.sfnt, tags::HMTX());
    if (hmtx_rec) {
        ByteReader tr = r.sub_reader(hmtx_rec->offset, hmtx_rec->length);
        auto res = parse_hmtx(tr, f.maxp.num_glyphs, f.hhea.number_of_h_metrics);
        if (res.ok()) f.hmtx = std::move(res.value);
    }

    // post
    const TableRecord* post_rec = find_table(f.sfnt, tags::POST());
    if (post_rec) {
        ByteReader tr = r.sub_reader(post_rec->offset, post_rec->length);
        auto res = parse_post(tr);
        if (res.ok()) f.post = res.value;
    }

    // OS/2
    const TableRecord* os2_rec = find_table(f.sfnt, tags::OS2());
    if (os2_rec) {
        ByteReader tr = r.sub_reader(os2_rec->offset, os2_rec->length);
        auto res = parse_os2(tr);
        if (res.ok()) { f.os2 = res.value; f.has_os2 = true; }
    }

    // name
    const TableRecord* name_rec = find_table(f.sfnt, tags::NAME());
    if (name_rec) {
        ByteReader tr = r.sub_reader(name_rec->offset, name_rec->length);
        auto res = parse_name(tr);
        if (res.ok()) { f.name = std::move(res.value); f.has_name = true; }
    }

    // cmap
    const TableRecord* cmap_rec = find_table(f.sfnt, tags::CMAP());
    if (cmap_rec) {
        ByteReader tr = r.sub_reader(cmap_rec->offset, cmap_rec->length);
        auto res = parse_cmap(tr);
        if (res.ok()) f.cmap = std::move(res.value);
    }

    // loca
    const TableRecord* loca_rec = find_table(f.sfnt, tags::LOCA());
    if (loca_rec) {
        ByteReader tr = r.sub_reader(loca_rec->offset, loca_rec->length);
        auto res = parse_loca(tr, f.maxp.num_glyphs, f.head.index_to_loc_format);
        if (res.ok()) f.loca = std::move(res.value);
    }

    // COLR
    const TableRecord* colr_rec = find_table(f.sfnt, tags::COLR());
    if (colr_rec) {
        ByteReader tr = r.sub_reader(colr_rec->offset, colr_rec->length);
        auto res = parse_colr(tr);
        if (res.ok()) { f.colr = std::move(res.value); f.has_colr = true; }
    }

    // CPAL
    const TableRecord* cpal_rec = find_table(f.sfnt, tags::CPAL());
    if (cpal_rec) {
        ByteReader tr = r.sub_reader(cpal_rec->offset, cpal_rec->length);
        auto res = parse_cpal(tr);
        if (res.ok()) { f.cpal = std::move(res.value); f.has_cpal = true; }
    }

    // fvar
    const TableRecord* fvar_rec = find_table(f.sfnt, tags::FVAR());
    if (fvar_rec) {
        ByteReader tr = r.sub_reader(fvar_rec->offset, fvar_rec->length);
        auto res = parse_fvar(tr);
        if (res.ok()) { f.fvar = std::move(res.value); f.has_fvar = true; }
    }

    // avar
    const TableRecord* avar_rec = find_table(f.sfnt, tags::AVAR());
    if (avar_rec) {
        ByteReader tr = r.sub_reader(avar_rec->offset, avar_rec->length);
        auto res = parse_avar(tr);
        if (res.ok()) { f.avar = std::move(res.value); f.has_avar = true; }
    }

    return Result<FontFace>::success(std::move(f));
}

void print_font_info(const FontFace& font) {
    const char* empty = "(unknown)";

    const std::string* family = font.has_name
        ? font.name.find(uint16_t(NameId::FamilyName)) : nullptr;
    const std::string* style  = font.has_name
        ? font.name.find(uint16_t(NameId::SubfamilyName)) : nullptr;
    const std::string* version_str = font.has_name
        ? font.name.find(uint16_t(NameId::Version)) : nullptr;

    printf("Family:       %s\n", family   ? family->c_str()    : empty);
    printf("Subfamily:    %s\n", style    ? style->c_str()     : empty);
    printf("Version:      %s\n", version_str ? version_str->c_str() : empty);
    printf("SFNT version: %s\n", sfnt_version_name(font.sfnt.sfVersion));
    printf("Tables:       %u\n", font.sfnt.numTables);
    printf("Glyphs:       %u\n", font.maxp.num_glyphs);
    printf("Units/em:     %u\n", font.head.units_per_em);
    printf("Bbox:         (%d, %d)–(%d, %d)\n",
           font.head.x_min, font.head.y_min,
           font.head.x_max, font.head.y_max);
    printf("Ascender:     %d\n", font.hhea.ascender);
    printf("Descender:    %d\n", font.hhea.descender);
    printf("Line gap:     %d\n", font.hhea.line_gap);

    if (font.has_os2) {
        printf("Typo asc:     %d\n", font.os2.s_typo_ascender);
        printf("Typo desc:    %d\n", font.os2.s_typo_descender);
        printf("Win asc:      %u\n", font.os2.us_win_ascent);
        printf("Win desc:     %u\n", font.os2.us_win_descent);
        printf("Weight:       %u\n", font.os2.us_weight_class);
    }

    if (font.has_colr)
        printf("Color glyphs: %zu (COLR v%u)\n",
               font.colr.glyphs.size(), font.colr.version);
    if (font.has_fvar)
        printf("Var axes:     %zu\n", font.fvar.axes.size());

    printf("Italic angle: %.3f\n", font.post.italic_angle.to_f64());
    printf("Fixed pitch:  %s\n", font.post.is_fixed_pitch ? "yes" : "no");
}

void print_tables(const FontFace& font) {
    printf("%-4s  %8s  %8s  %8s  %s\n",
           "Tag", "Offset", "Length", "Checksum", "OK");
    printf("%-4s  %8s  %8s  %8s  %s\n",
           "----", "--------", "--------", "--------", "--");
    for (const auto& t : font.sfnt.tables) {
        char tag[5];
        tag[0] = char((t.tag.value >> 24) & 0xFF);
        tag[1] = char((t.tag.value >> 16) & 0xFF);
        tag[2] = char((t.tag.value >>  8) & 0xFF);
        tag[3] = char((t.tag.value      ) & 0xFF);
        tag[4] = '\0';
        bool ok = verify_table_checksum(t, font.raw_data.data(),
                                        font.raw_data.size());
        printf("%-4s  %8u  %8u  %08X  %s\n",
               tag, t.offset, t.length, t.checksum, ok ? "ok" : "BAD");
    }
}

Result<RawGlyph> load_glyph(const FontFace& font, uint16_t glyph_id) {
    if (glyph_id >= font.maxp.num_glyphs)
        return Result<RawGlyph>::error(Status::InvalidGlyphId);
    if (font.loca.offsets.empty())
        return Result<RawGlyph>::error(Status::TableNotFound);

    GlyfRange range = font.loca.glyph_range(glyph_id);

    const TableRecord* glyf_rec = find_table(font.sfnt, tags::GLYF());
    if (!glyf_rec) return Result<RawGlyph>::error(Status::TableNotFound);

    uint32_t abs_off = glyf_rec->offset + range.begin;
    uint32_t len     = range.end - range.begin;

    ByteReader r(font.raw_data.data(), font.raw_data.size());
    ByteReader gr = r.sub_reader(abs_off, len);
    return parse_glyph(gr);
}

uint16_t codepoint_to_glyph(const FontFace& font, uint32_t codepoint) {
    return font.cmap.lookup(codepoint);
}

} // namespace fontscope
