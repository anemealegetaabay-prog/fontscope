#include "fontscope/hint_vm.h"
#include <cassert>
#include <cstdio>

using namespace fontscope;

static void test_push_pop() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // PUSHB[0] 42
    uint8_t code[] = {0xB0, 42};
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(ctx.stack.size() == 1);
    assert(ctx.stack[0] == 42);
}

static void test_stack_arithmetic() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // PUSHB[1] 10 20  ADD
    uint8_t code[] = {0xB1, 10, 20, 0x60};
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(ctx.stack.size() == 1);
    assert(ctx.stack[0] == 30);
}

static void test_storage() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 8, cvt);

    // PUSHW[0] value=99  PUSHB[0] index=3  WS  PUSHB[0] 3  RS
    uint8_t code[] = {
        0xB8, 0x00, 99,  // PUSHW: push 99
        0xB0, 3,         // PUSHB: push 3 (index)
        0x42,            // WS: pop index, pop value → storage[3]=99
        0xB0, 3,         // PUSHB: push 3
        0x43             // RS: push storage[3]
    };
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(!ctx.stack.empty() && ctx.stack.back() == 99);
}

static void test_svtca() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // SVTCA[y] (0x00) sets freedom/projection to Y axis
    uint8_t code[] = {0x00};
    FpgmTable fpgm;
    execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(ctx.gs.freedom_vector_x.raw == 0);
    assert(ctx.gs.freedom_vector_y.raw != 0);
}

static void test_if_taken() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // Push 1 (true), IF, push 77, EIF
    uint8_t code[] = {0xB0, 1, 0x58, 0xB0, 77, 0x59};
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(!ctx.stack.empty() && ctx.stack.back() == 77);
}

static void test_if_not_taken() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // Push 0 (false), IF, push 77, EIF
    uint8_t code[] = {0xB0, 0, 0x58, 0xB0, 77, 0x59};
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(ctx.stack.empty());
}

static void test_max_iter_guard() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // Infinite loop: 0x40 (NPUSHB) with count=0 is safe (pushes nothing)
    // Build a loop that just clears and re-pushes but will hit iter limit
    // by having many NOPs (0x59 = EIF which is fine as no-op here)
    std::vector<uint8_t> code(200001, 0x59);  // EIF as no-op
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code.data(), code.size(), fpgm, 512, 100000);
    assert(r == ExecResult::Abort);
}

static void test_shz_bounds() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    // zone_pts[1] matches actual zone size — no overflow should occur
    ctx.reset(4, 2, 4, cvt, 4, 2);

    // SRP2 = 0  (for the ref point in zone 0)
    // SZP2 = 0  (zp2 = twilight zone)
    // SHZ[1] (zone 1 = glyph zone)
    // Ref point 0 in twilight zone has no displacement → delta=0 → no writes
    uint8_t code[] = {
        0xB0, 0,   // push 0
        0x12,      // SRP2
        0xB0, 0,   // push 0
        0x15,      // SZP2 = zone 0
        0x36       // SHZ zone_id=1
    };
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
}

// maxp.maxPoints (zone_pts) is usually larger than the current glyph's point
// count. SHZ must shift only the points that exist, not write past the zone.
static void test_shz_clamped_to_zone_size() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(0, 2, 4, cvt, 242, 2);  // as make_hint_context() does
    ctx.zones[1].resize(15);          // as glyph loading does
    // Move twilight point 0 by +1 in x so SHZ has a non-zero delta.
    ctx.zones[0].x_coords[0] = Fixed16::from_int(1);

    uint8_t code[] = {
        0xB0, 0,   // push 0
        0x12,      // SRP2 = 0
        0xB0, 0,   // push 0
        0x15,      // SZP2 = zone 0 (twilight)
        0x36       // SHZ zone_id=1
    };
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(ctx.zones[1].size() == 15);
    for (size_t i = 0; i < ctx.zones[1].size(); ++i)
        assert(ctx.zones[1].x_coords[i].raw == Fixed16::from_int(1).raw);
}

static void test_div_negative_dividend() {
    HintContext ctx;
    std::vector<Fixed16> cvt;
    ctx.reset(4, 2, 4, cvt);

    // PUSHW -1 (dividend), PUSHB 1 (divisor), DIV. The 26.6 fixed-point divide
    // scales the dividend left by 6; with a negative dividend that must not
    // invoke left-shift-of-negative UB.
    uint8_t code[] = {
        0xB8, 0xFF, 0xFF,  // PUSHW -1
        0xB0, 1,           // PUSHB 1
        0x62               // DIV
    };
    FpgmTable fpgm;
    auto r = execute_hint_program(ctx, code, sizeof(code), fpgm);
    assert(r == ExecResult::Ok);
    assert(!ctx.stack.empty() && ctx.stack.back() == -64);  // (-1 * 64) / 1
}

int main() {
    test_push_pop();
    test_stack_arithmetic();
    test_storage();
    test_svtca();
    test_if_taken();
    test_if_not_taken();
    test_max_iter_guard();
    test_shz_bounds();
    test_shz_clamped_to_zone_size();
    test_div_negative_dividend();
    puts("test_hint_vm: all passed");
    return 0;
}
