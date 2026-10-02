#include "fontscope/subsetter.h"
#include "fontscope/sfnt.h"
#include "fontscope/cmap.h"
#include <cstdio>
#include <cassert>
#include <cstring>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

//

static void test_emit_u16() {
    std::vector<uint8_t> out;
    emit_u16(out, 0x1234);
    CHECK(out.size() == 2);
    CHECK(out[0] == 0x12);
    CHECK(out[1] == 0x34);
}

static void test_emit_u32() {
    std::vector<uint8_t> out;
    emit_u32(out, 0xDEADBEEF);
    CHECK(out.size() == 4);
    CHECK(out[0] == 0xDE);
    CHECK(out[1] == 0xAD);
    CHECK(out[2] == 0xBE);
    CHECK(out[3] == 0xEF);
}

static void test_patch_u32() {
    std::vector<uint8_t> out(8, 0);
    patch_u32(out, 2, 0x12345678);
    CHECK(out[2] == 0x12);
    CHECK(out[3] == 0x34);
    CHECK(out[4] == 0x56);
    CHECK(out[5] == 0x78);
    // Unchanged.
    CHECK(out[0] == 0);
    CHECK(out[1] == 0);
}

static void test_patch_u32_oob() {
    std::vector<uint8_t> out(4, 0);
    // Writing at offset 2 would write to [2..5] but out is only 4 bytes.
    patch_u32(out, 2, 0xABCD1234);
    // Should not crash; no full write.
    CHECK(out.size() == 4);
}

static void test_sfnt_checksum_zero() {
    uint8_t data[8] = {};
    CHECK(sfnt_checksum(data, 8) == 0);
}

static void test_sfnt_checksum_known() {
    // "head" = 0x68656164; treated as one 32-bit word.
    uint8_t data[4] = {0x68, 0x65, 0x61, 0x64};
    uint32_t sum = sfnt_checksum(data, 4);
    CHECK(sum == 0x68656164u);
}

static void test_sfnt_checksum_pad() {
    // 5 bytes: [0x01, 0x02, 0x03, 0x04, 0x05] → two 32-bit words.
    uint8_t data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t sum = sfnt_checksum(data, 5);
    // Word 1: 0x01020304, word 2: 0x05000000.
    CHECK(sum == 0x01020304u + 0x05000000u);
}

static void test_pad_to_4() {
    std::vector<uint8_t> v = {0x01, 0x02, 0x03};
    pad_to_4(v);
    CHECK(v.size() == 4);
    CHECK(v[3] == 0);

    std::vector<uint8_t> v4 = {0x01, 0x02, 0x03, 0x04};
    pad_to_4(v4);
    CHECK(v4.size() == 4);  // already aligned

    std::vector<uint8_t> v5 = {1, 2, 3, 4, 5};
    pad_to_4(v5);
    CHECK(v5.size() == 8);
}

//

static void test_build_glyph_order() {
    std::unordered_set<uint16_t> gids = {5, 1, 3, 10, 2};
    auto order = build_glyph_order(gids);
    CHECK(order.size() == 5);
    // Sorted ascending.
    for (size_t i = 1; i < order.size(); ++i)
        CHECK(order[i] > order[i-1]);
    CHECK(order[0] == 1);
    CHECK(order[4] == 10);
}

static void test_build_gid_map() {
    std::vector<uint16_t> order = {1, 3, 5, 10};
    auto m = build_gid_map(order);
    CHECK(m.size() == 4);
    CHECK(m.at(1) == 0);
    CHECK(m.at(3) == 1);
    CHECK(m.at(5) == 2);
    CHECK(m.at(10) == 3);
}

static void test_build_gid_map_includes_zero() {
    // Always include glyph 0 (notdef).
    std::unordered_set<uint16_t> gids = {0, 5, 10};
    auto order = build_glyph_order(gids);
    CHECK(order[0] == 0);
    auto m = build_gid_map(order);
    CHECK(m.at(0) == 0);
    CHECK(m.at(5) == 1);
    CHECK(m.at(10) == 2);
}

static void test_emit_sequence() {
    std::vector<uint8_t> out;
    emit_u16(out, 1);
    emit_u16(out, 2);
    emit_u32(out, 0x0A0B0C0Du);
    CHECK(out.size() == 8);
    CHECK(out[0] == 0 && out[1] == 1);
    CHECK(out[2] == 0 && out[3] == 2);
    CHECK(out[4] == 0x0A && out[5] == 0x0B && out[6] == 0x0C && out[7] == 0x0D);
}

static void test_sfnt_checksum_words() {
    // Two words: 0x00010000 + 0x4F54544F = 0x4F55544F.
    uint8_t data[8] = {0x00, 0x01, 0x00, 0x00, 0x4F, 0x54, 0x54, 0x4F};
    uint32_t sum = sfnt_checksum(data, 8);
    CHECK(sum == 0x00010000u + 0x4F54544Fu);
}

static void test_pad_to_4_already_aligned() {
    std::vector<uint8_t> v(8, 0xFF);
    pad_to_4(v);
    CHECK(v.size() == 8);
    for (auto b : v) CHECK(b == 0xFF);
}

//

// Parse the cmap table back out of a subset's SFNT bytes.
static CmapIndex subset_cmap(const SubsetResult& res) {
    ByteReader r(res.sfnt_data.data(), res.sfnt_data.size());
    auto hdr = parse_sfnt_header(r);
    CHECK(hdr.ok());
    const TableRecord* rec = hdr.ok() ? find_table(hdr.value, tags::CMAP()) : nullptr;
    CHECK(rec != nullptr);
    if (!rec) return {};

    // The format-4 length field (after the 12-byte cmap header and encoding
    // record) must cover the whole subtable.
    ByteReader len_r = r.sub_reader(rec->offset + 14, 2);
    CHECK(len_r.read_u16_be() == rec->length - 12);

    ByteReader cr = r.sub_reader(rec->offset, rec->length);
    auto cmap = parse_cmap(cr);
    CHECK(cmap.ok());
    return cmap.value;
}

// A FontFace with only a cmap: with no glyf/loca, the requested glyphs are
// kept as-is and the subset contains just the rebuilt cmap.
static FontFace make_cmap_only_font() {
    FontFace f{};
    f.cmap.unicode_to_glyph = {
        {'A', 36}, {'B', 37}, {'C', 38}, {0x20AC, 120}, {0x1F600, 200},
    };
    return f;
}

static void test_subset_font_builds_cmap() {
    FontFace f = make_cmap_only_font();
    // 'B' twice, 'Z' unmapped, U+1F600 outside the BMP.
    auto res = subset_font(f, {'A', 'B', 'B', 'C', 'Z', 0x20AC, 0x1F600});
    CHECK(res.ok);
    // notdef + old GIDs 36, 37, 38, 120, 200 → new GIDs 0..5 in old-GID order.
    CHECK(res.num_glyphs == 6);

    CmapIndex cmap = subset_cmap(res);
    CHECK(cmap.subtables.size() == 1);
    CHECK(!cmap.subtables.empty() && cmap.subtables[0].platform_id == 3 &&
          cmap.subtables[0].encoding_id == 1 && cmap.subtables[0].format == 4);
    CHECK(cmap.lookup('A') == 1);
    CHECK(cmap.lookup('B') == 2);
    CHECK(cmap.lookup('C') == 3);
    CHECK(cmap.lookup(0x20AC) == 4);
    CHECK(cmap.lookup('Z') == 0);
    CHECK(cmap.lookup(0x1F600) == 0);  // not representable in format 4
    CHECK(cmap.unicode_to_glyph.size() == 4);
}

// A head table that ends inside indexToLocFormat (offset 50) must be copied
// unpatched instead of being written past its end.
static void test_subset_short_head_table() {
    FontFace f = make_cmap_only_font();
    f.raw_data.assign(64, 0xAB);
    TableRecord head{};
    head.tag    = tags::HEAD();
    head.offset = 0;
    head.length = 51;
    f.sfnt.tables.push_back(head);

    auto res = subset_font(f, {'A'});
    CHECK(res.ok);
    ByteReader r(res.sfnt_data.data(), res.sfnt_data.size());
    auto hdr = parse_sfnt_header(r);
    CHECK(hdr.ok());
    const TableRecord* rec = hdr.ok() ? find_table(hdr.value, tags::HEAD()) : nullptr;
    CHECK(rec != nullptr && rec->length == 51);
    if (rec) CHECK(res.sfnt_data[rec->offset + 50] == 0xAB);  // not patched
}

static void test_subset_font_by_gids_empty_cmap() {
    FontFace f = make_cmap_only_font();
    auto res = subset_font_by_gids(f, {36, 37});
    CHECK(res.ok);
    CmapIndex cmap = subset_cmap(res);
    CHECK(cmap.unicode_to_glyph.empty());
}

int main() {
    test_emit_u16();
    test_emit_u32();
    test_patch_u32();
    test_patch_u32_oob();
    test_sfnt_checksum_zero();
    test_sfnt_checksum_known();
    test_sfnt_checksum_pad();
    test_pad_to_4();
    test_build_glyph_order();
    test_build_gid_map();
    test_build_gid_map_includes_zero();
    test_emit_sequence();
    test_sfnt_checksum_words();
    test_pad_to_4_already_aligned();
    test_subset_font_builds_cmap();
    test_subset_font_by_gids_empty_cmap();
    test_subset_short_head_table();

    if (failures) {
        fprintf(stderr, "test_subsetter: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_subsetter: all passed\n");
    return 0;
}
