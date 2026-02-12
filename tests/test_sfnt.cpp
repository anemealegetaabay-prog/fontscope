#include "fontscope/sfnt.h"
#include "fontscope/tables.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace fontscope;

// Build a minimal TrueType sfnt header with two table records.
static std::vector<uint8_t> make_sfnt(uint16_t num_tables) {
    std::vector<uint8_t> buf;
    auto pu32 = [&](uint32_t v) {
        buf.push_back(v >> 24); buf.push_back(v >> 16);
        buf.push_back(v >> 8);  buf.push_back(v);
    };
    auto pu16 = [&](uint16_t v) {
        buf.push_back(v >> 8); buf.push_back(v);
    };

    pu32(0x00010000u);  // TrueType sfVersion
    pu16(num_tables);
    pu16(0); pu16(0); pu16(0);  // searchRange, entrySelector, rangeShift

    // Add two fake table records.
    for (uint16_t i = 0; i < num_tables; ++i) {
        uint32_t tag = (i == 0) ? 0x68656164u  // 'head'
                                : 0x6D61787075; // 'maxp'
        pu32(uint32_t(tag));
        pu32(0xDEADBEEFu);  // fake checksum
        pu32(100u + i * 64u);
        pu32(54u);
    }
    return buf;
}

static void test_parse_valid() {
    auto buf = make_sfnt(2);
    ByteReader r(buf.data(), buf.size());
    auto res = parse_sfnt_header(r);
    assert(res.ok());
    assert(res.value.sfVersion == 0x00010000u);
    assert(res.value.numTables == 2);
    assert(res.value.tables.size() == 2);
    assert(res.value.tables[0].tag == tags::HEAD());
}

static void test_find_table() {
    auto buf = make_sfnt(2);
    ByteReader r(buf.data(), buf.size());
    auto res = parse_sfnt_header(r);
    assert(res.ok());

    const TableRecord* head = find_table(res.value, tags::HEAD());
    assert(head != nullptr);
    assert(head->offset == 100u);

    const TableRecord* loca = find_table(res.value, tags::LOCA());
    assert(loca == nullptr);
}

static void test_invalid_magic() {
    uint8_t buf[12] = {};
    buf[0] = 0xDE; buf[1] = 0xAD; buf[2] = 0xBE; buf[3] = 0xEF;
    buf[5] = 1;  // numTables=1
    ByteReader r(buf, sizeof(buf));
    auto res = parse_sfnt_header(r);
    assert(!res.ok());
    assert(res.status == Status::InvalidMagic);
}

static void test_truncated() {
    uint8_t buf[4] = {0x00, 0x01, 0x00, 0x00};
    ByteReader r(buf, sizeof(buf));
    auto res = parse_sfnt_header(r);
    assert(!res.ok());
}

static void test_cff_version() {
    auto buf = make_sfnt(1);
    // Patch sfVersion to CFF 'OTTO'.
    buf[0] = 0x4F; buf[1] = 0x54; buf[2] = 0x54; buf[3] = 0x4F;
    ByteReader r(buf.data(), buf.size());
    auto res = parse_sfnt_header(r);
    assert(res.ok());
    assert(res.value.sfVersion == 0x4F54544Fu);
    assert(sfnt_version_name(res.value.sfVersion) == std::string("CFF/OTTO"));
}

static void test_version_names() {
    assert(sfnt_version_is_valid(0x00010000u));
    assert(sfnt_version_is_valid(0x4F54544Fu));
    assert(!sfnt_version_is_valid(0x12345678u));
}

int main() {
    test_parse_valid();
    test_find_table();
    test_invalid_magic();
    test_truncated();
    test_cff_version();
    test_version_names();
    puts("test_sfnt: all passed");
    return 0;
}
