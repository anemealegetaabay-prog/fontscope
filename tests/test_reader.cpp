#include "fontscope/reader.h"
#include <cassert>
#include <cstdio>

using namespace fontscope;

static void test_basic_reads() {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ByteReader r(data, sizeof(data));
    assert(r.ok());
    assert(r.remaining() == 8);
    assert(r.read_u8() == 0x01);
    assert(r.read_u16_be() == 0x0203);
    assert(r.read_u32_be() == 0x04050607);
    assert(r.read_u8() == 0x08);
    assert(r.ok());
    assert(r.remaining() == 0);
}

static void test_truncated_read() {
    uint8_t data[] = {0x01, 0x02};
    ByteReader r(data, sizeof(data));
    r.read_u32_be();
    assert(!r.ok());
}

static void test_signed_reads() {
    uint8_t data[] = {0xFF, 0x80, 0xFF, 0x7F};
    ByteReader r(data, sizeof(data));
    assert(r.read_i8()    == -1);
    assert(r.read_i8()    == int8_t(-128));
    assert(r.read_i16_be()== int16_t(0xFF7F));
    assert(r.ok());
}

static void test_skip() {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    ByteReader r(data, sizeof(data));
    r.skip(2);
    assert(r.read_u16_be() == 0xCCDD);
    assert(r.ok());
}

static void test_sub_reader() {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    ByteReader r(data, sizeof(data));
    ByteReader sub = r.sub_reader(1, 3);
    assert(sub.remaining() == 3);
    assert(sub.read_u8() == 0x01);
    assert(sub.read_u16_be() == 0x0203);
    assert(sub.ok());
}

static void test_sub_reader_oob() {
    uint8_t data[] = {0x01, 0x02};
    ByteReader r(data, sizeof(data));
    ByteReader sub = r.sub_reader(0, 10);  // extends past end
    assert(sub.remaining() == 0);
}

static void test_seek() {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
    ByteReader r(data, sizeof(data));
    r.seek(2);
    assert(r.read_u16_be() == 0x3040);
    assert(r.ok());
}

static void test_u24() {
    uint8_t data[] = {0x01, 0x02, 0x03};
    ByteReader r(data, sizeof(data));
    assert(r.read_u24_be() == 0x010203u);
    assert(r.ok());
}

int main() {
    test_basic_reads();
    test_truncated_read();
    test_signed_reads();
    test_skip();
    test_sub_reader();
    test_sub_reader_oob();
    test_seek();
    test_u24();
    puts("test_reader: all passed");
    return 0;
}
