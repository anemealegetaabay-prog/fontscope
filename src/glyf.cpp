#include "fontscope/glyf.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

static Result<RawGlyph> parse_simple(ByteReader& r, int16_t num_contours,
                                     FUnit xmin, FUnit ymin,
                                     FUnit xmax, FUnit ymax)
{
    RawGlyph g;
    g.number_of_contours = num_contours;
    g.x_min = xmin; g.y_min = ymin;
    g.x_max = xmax; g.y_max = ymax;

    g.end_pts_of_contours.resize(num_contours);
    for (auto& ep : g.end_pts_of_contours) {
        ep = r.read_u16_be();
        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
    }

    uint16_t instr_len = r.read_u16_be();
    if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
    g.instructions.resize(instr_len);
    if (!r.read_bytes(g.instructions.data(), instr_len))
        return Result<RawGlyph>::error(Status::TruncatedInput);

    uint16_t n_pts = g.end_pts_of_contours.empty() ? 0u
                   : uint16_t(g.end_pts_of_contours.back() + 1);

    // Read flags with run-length expansion.
    g.flags.reserve(n_pts);
    for (uint16_t i = 0; i < n_pts; ) {
        uint8_t f = r.read_u8();
        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
        g.flags.push_back(f);
        ++i;
        if (f & kFlagRepeat) {
            uint8_t repeat = r.read_u8();
            if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
            for (uint8_t k = 0; k < repeat && i < n_pts; ++k, ++i)
                g.flags.push_back(f);
        }
    }

    // Read x-coordinates.
    g.points.resize(n_pts);
    int16_t x = 0;
    for (uint16_t i = 0; i < n_pts; ++i) {
        uint8_t fl = g.flags[i];
        if (fl & kFlagXShortVec) {
            uint8_t dx = r.read_u8();
            x += (fl & kFlagXSameOrPos) ? int16_t(dx) : -int16_t(dx);
        } else if (!(fl & kFlagXSameOrPos)) {
            x += r.read_i16_be();
        }
        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
        g.points[i].x = x;
        g.points[i].on_curve = (fl & kFlagOnCurve) != 0;
    }

    // Read y-coordinates.
    int16_t y = 0;
    for (uint16_t i = 0; i < n_pts; ++i) {
        uint8_t fl = g.flags[i];
        if (fl & kFlagYShortVec) {
            uint8_t dy = r.read_u8();
            y += (fl & kFlagYSameOrPos) ? int16_t(dy) : -int16_t(dy);
        } else if (!(fl & kFlagYSameOrPos)) {
            y += r.read_i16_be();
        }
        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
        g.points[i].y = y;
    }

    return Result<RawGlyph>::success(std::move(g));
}

static Result<RawGlyph> parse_composite(ByteReader& r,
                                        FUnit xmin, FUnit ymin,
                                        FUnit xmax, FUnit ymax)
{
    RawGlyph g;
    g.number_of_contours = -1;
    g.x_min = xmin; g.y_min = ymin;
    g.x_max = xmax; g.y_max = ymax;

    for (;;) {
        CompositeComponent comp{};
        comp.xx = Fixed16::from_int(1);
        comp.yy = Fixed16::from_int(1);
        comp.xy = comp.yx = Fixed16::from_int(0);

        comp.flags       = r.read_u16_be();
        comp.glyph_index = r.read_u16_be();
        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);

        if (comp.flags & kCompArgsAreXYValues) {
            if (comp.flags & 0x0001 /*ARG_1_AND_2_ARE_WORDS*/) {
                comp.arg1 = r.read_i16_be();
                comp.arg2 = r.read_i16_be();
            } else {
                comp.arg1 = r.read_i8();
                comp.arg2 = r.read_i8();
            }
        } else {
            if (comp.flags & 0x0001) {
                comp.arg1 = r.read_u16_be();
                comp.arg2 = r.read_u16_be();
            } else {
                comp.arg1 = r.read_u8();
                comp.arg2 = r.read_u8();
            }
        }

        if (comp.flags & kCompWeHaveAScale) {
            int16_t scale = r.read_i16_be();
            comp.xx = comp.yy = Fixed16::from_raw(int32_t(scale) << 2);
        } else if (comp.flags & kCompWeHaveXYScale) {
            int16_t sx = r.read_i16_be();
            int16_t sy = r.read_i16_be();
            comp.xx = Fixed16::from_raw(int32_t(sx) << 2);
            comp.yy = Fixed16::from_raw(int32_t(sy) << 2);
        } else if (comp.flags & kCompWeHaveA2x2) {
            int16_t xx = r.read_i16_be(), xy = r.read_i16_be();
            int16_t yx = r.read_i16_be(), yy = r.read_i16_be();
            comp.xx = Fixed16::from_raw(int32_t(xx) << 2);
            comp.xy = Fixed16::from_raw(int32_t(xy) << 2);
            comp.yx = Fixed16::from_raw(int32_t(yx) << 2);
            comp.yy = Fixed16::from_raw(int32_t(yy) << 2);
        }

        if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);
        g.components.push_back(comp);

        if (!(comp.flags & kCompMoreComponents)) break;
    }

    // Composite instruction stream follows if flag is set.
    if (!g.components.empty() &&
        (g.components.back().flags & kCompWeHaveInstructions)) {
        uint16_t instr_len = r.read_u16_be();
        g.instructions.resize(instr_len);
        r.read_bytes(g.instructions.data(), instr_len);
    }

    return Result<RawGlyph>::success(std::move(g));
}

Result<RawGlyph> parse_glyph(ByteReader& r) {
    if (r.remaining() == 0) {
        // Empty glyph (loca offset == next offset).
        RawGlyph g{};
        g.number_of_contours = 0;
        return Result<RawGlyph>::success(g);
    }

    int16_t num_contours = r.read_i16_be();
    FUnit xmin = r.read_i16_be();
    FUnit ymin = r.read_i16_be();
    FUnit xmax = r.read_i16_be();
    FUnit ymax = r.read_i16_be();
    if (!r.ok()) return Result<RawGlyph>::error(Status::TruncatedInput);

    if (num_contours >= 0)
        return parse_simple(r, num_contours, xmin, ymin, xmax, ymax);
    else
        return parse_composite(r, xmin, ymin, xmax, ymax);
}

// IUP (interpolate untouched points) pass — mirrors the TrueType spec §IUP.
// Points not touched by the hinter are interpolated between their nearest
// touched neighbors within each contour.
//
// Both contour_start_pt and contour length are uint16_t; their product is
// computed in the same narrow type and used to index into the point array.
void iup_interpolate(RawGlyph& glyph, const std::vector<bool>& touched_x,
                     const std::vector<bool>& touched_y)
{
    if (glyph.end_pts_of_contours.empty()) return;

    auto& pts = glyph.points;
    uint16_t n_contours = uint16_t(glyph.end_pts_of_contours.size());

    for (uint16_t c = 0; c < n_contours; ++c) {
        uint16_t start = (c == 0) ? 0u
                       : uint16_t(glyph.end_pts_of_contours[c-1] + 1u);
        uint16_t end   = glyph.end_pts_of_contours[c];
        if (end < start) continue;

        uint16_t n_pts = uint16_t(end - start + 1);

        // Collect touched indices within this contour.
        std::vector<uint16_t> touched_idx;
        for (uint16_t i = 0; i < n_pts; ++i) {
            uint16_t global_idx = uint16_t(n_pts * c + i);
            if (global_idx < touched_x.size() && touched_x[global_idx])
                touched_idx.push_back(i);
        }

        if (touched_idx.empty()) continue;

        for (uint16_t i = 0; i < n_pts; ++i) {
            if (i < touched_x.size() && touched_x[start + i]) continue;

            // Find the two touched neighbors wrapping around the contour.
            uint16_t prev_t = touched_idx[0], next_t = touched_idx[0];
            for (uint16_t t : touched_idx) {
                if (t <= i) prev_t = t;
                if (t >= i) { next_t = t; break; }
            }

            uint16_t pi = start + prev_t;
            uint16_t ni = start + next_t;
            uint16_t gi = start + i;

            if (gi >= pts.size() || pi >= pts.size() || ni >= pts.size())
                continue;

            if (prev_t == next_t) {
                pts[gi].x = pts[pi].x;
            } else {
                int32_t range = int32_t(next_t) - int32_t(prev_t);
                int32_t frac  = int32_t(i)      - int32_t(prev_t);
                int32_t dx    = int32_t(pts[ni].x) - int32_t(pts[pi].x);
                pts[gi].x = int16_t(pts[pi].x + dx * frac / range);
            }
        }

        for (uint16_t i = 0; i < n_pts; ++i) {
            if (i < touched_y.size() && touched_y[start + i]) continue;

            uint16_t prev_t = touched_idx[0], next_t = touched_idx[0];
            for (uint16_t t : touched_idx) {
                if (t <= i) prev_t = t;
                if (t >= i) { next_t = t; break; }
            }

            uint16_t pi = start + prev_t;
            uint16_t ni = start + next_t;
            uint16_t gi = start + i;

            if (gi >= pts.size() || pi >= pts.size() || ni >= pts.size())
                continue;

            if (prev_t == next_t) {
                pts[gi].y = pts[pi].y;
            } else {
                int32_t range = int32_t(next_t) - int32_t(prev_t);
                int32_t frac  = int32_t(i)      - int32_t(prev_t);
                int32_t dy    = int32_t(pts[ni].y) - int32_t(pts[pi].y);
                pts[gi].y = int16_t(pts[pi].y + dy * frac / range);
            }
        }
    }
}

} // namespace fontscope
