#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "fontscope/errors.h"
#include "fontscope/fixed.h"
#include "fontscope/font_types.h"

namespace fontscope {

// TrueType graphics state, per spec §Graphics State Variables.
struct GraphicsState {
    uint8_t  auto_flip{true};
    Fixed16  control_value_cut_in{Fixed16::from_raw(68)};  // 17/16
    Fixed16  delta_base{Fixed16::from_int(9)};
    Fixed16  delta_shift{Fixed16::from_int(3)};
    Fixed16  freedom_vector_x{Fixed16::from_int(1)};
    Fixed16  freedom_vector_y{Fixed16::from_int(0)};
    Fixed16  projection_vector_x{Fixed16::from_int(1)};
    Fixed16  projection_vector_y{Fixed16::from_int(0)};
    Fixed16  dual_proj_vector_x{Fixed16::from_int(1)};
    Fixed16  dual_proj_vector_y{Fixed16::from_int(0)};
    uint8_t  round_state{1};        // 0=off, 1=to-grid, 2=half, 3=double-grid
    Fixed16  single_width_cut_in{};
    Fixed16  single_width_value{};
    uint32_t loop{1};
    uint32_t min_distance{1};
    uint32_t rp0{0}, rp1{0}, rp2{0};
    uint32_t zp0{1}, zp1{1}, zp2{1};
    Fixed16  scan_control{};
    Fixed16  scan_type{};
};

// A zone of glyph points (zone 0 = twilight, zone 1 = glyph).
struct PointZone {
    std::vector<Fixed16> x_coords;
    std::vector<Fixed16> y_coords;
    std::vector<Fixed16> ox_coords;  // original (pre-hint)
    std::vector<Fixed16> oy_coords;
    std::vector<bool>    touched_x;
    std::vector<bool>    touched_y;

    size_t size() const { return x_coords.size(); }

    void resize(size_t n) {
        x_coords.assign(n, Fixed16{});
        y_coords.assign(n, Fixed16{});
        ox_coords.assign(n, Fixed16{});
        oy_coords.assign(n, Fixed16{});
        touched_x.assign(n, false);
        touched_y.assign(n, false);
    }
};

// The full hinting VM execution context.
struct HintContext {
    GraphicsState        gs;
    std::vector<int32_t> stack;
    std::vector<Fixed16> cvt;
    std::vector<int32_t> storage;
    PointZone            zones[2];    // 0 = twilight, 1 = glyph
    uint16_t             num_glyph_pts{0};
    uint32_t             zone_pts[2]{0, 0};  // per-zone upper bound from maxp

    void reset(uint16_t glyph_pt_count, uint16_t twilight_count,
               uint32_t storage_count, const std::vector<Fixed16>& cvt_init,
               uint32_t max_glyph_pts = 0, uint32_t max_twilight_pts = 0);
};

// Execution result from running a hint program.
enum class ExecResult { Ok, StackUnderflow, StackOverflow, BadOpcode,
                        BadZone, BadCvtIndex, CallDepthExceeded, Abort };

using FpgmTable = std::vector<uint8_t>;

// Execute a TrueType bytecode program. Returns ExecResult::Ok on success.
ExecResult execute_hint_program(HintContext& ctx, const uint8_t* code,
                                size_t code_len, const FpgmTable& fpgm,
                                uint32_t max_stack = 512,
                                uint32_t max_iters = 100000);

} // namespace fontscope
