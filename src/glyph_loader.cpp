#include "fontscope/glyph_loader.h"
#include "fontscope/sfnt.h"
#include "fontscope/tables.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

static constexpr int kMaxCompositeDepth = 8;

// Largest CVT table any real font ships; prevents forged table lengths from
// forcing multi-gigabyte vector reservations during hint-context setup.
static constexpr uint32_t kMaxCvtEntries = 1u << 14;
static constexpr uint32_t kMaxHintStorage = 1u << 12;

HintContext make_hint_context(const FontFace& font, uint16_t glyph_id, uint16_t ppem) {
    HintContext ctx;

    // Load CVT values from the cvt  table if present.
    std::vector<Fixed16> cvt_init;
    const TableRecord* cvt_rec = find_table(font.sfnt, tags::CVT());
    if (cvt_rec) {
        ByteReader cr(font.raw_data.data(), font.raw_data.size());
        ByteReader sub = cr.sub_reader(cvt_rec->offset, cvt_rec->length);
        uint32_t n_cvt = cvt_rec->length / 2;
        if (cvt_rec->offset < font.raw_data.size()) {
            uint32_t avail = uint32_t((font.raw_data.size() - cvt_rec->offset) / 2);
            if (n_cvt > avail) n_cvt = avail;
        } else {
            n_cvt = 0;
        }
        if (n_cvt > kMaxCvtEntries) n_cvt = kMaxCvtEntries;
        cvt_init.reserve(n_cvt);
        for (uint32_t i = 0; i < n_cvt; ++i) {
            int16_t v = sub.read_i16_be();
            if (!sub.ok()) break;
            // Scale CVT value from FUnits to pixels at current ppem.
            double scaled = double(v) * double(ppem) / double(font.head.units_per_em);
            cvt_init.push_back(Fixed16::from_f64(scaled));
        }
    }

    const auto& mp = font.maxp;
    uint32_t storage  = mp.max_storage       > 0 ? mp.max_storage       : 64u;
    if (storage > kMaxHintStorage) storage = kMaxHintStorage;
    uint16_t twilight = mp.max_twilight_points > 0 ? mp.max_twilight_points : 8u;

    // Use maxp limits as zone_pts so hints can reference up to the declared max.
    uint32_t max_glyph = mp.max_points   > 0 ? mp.max_points   : 256u;
    uint32_t max_twi   = mp.max_twilight_points > 0 ? mp.max_twilight_points : 8u;

    ctx.reset(0, twilight, storage, cvt_init, max_glyph, max_twi);
    (void)glyph_id;
    return ctx;
}

static void apply_transform(ProcessedGlyph& pg, const CompositeComponent& comp) {
    for (auto& pt : pg.points) {
        Fixed16 x = Fixed16::from_int(pt.x);
        Fixed16 y = Fixed16::from_int(pt.y);
        pt.x = FUnit((comp.xx * x + comp.xy * y).integer() + comp.arg1);
        pt.y = FUnit((comp.yx * x + comp.yy * y).integer() + comp.arg2);
    }
    // Update bounding box.
    if (!pg.points.empty()) {
        pg.x_min = pg.x_max = pg.points[0].x;
        pg.y_min = pg.y_max = pg.points[0].y;
        for (const auto& pt : pg.points) {
            pg.x_min = std::min(pg.x_min, pt.x);
            pg.x_max = std::max(pg.x_max, pt.x);
            pg.y_min = std::min(pg.y_min, pt.y);
            pg.y_max = std::max(pg.y_max, pt.y);
        }
    }
}

Result<ProcessedGlyph> resolve_composite(const FontFace& font,
                                          HintContext& hint_ctx,
                                          const VariationStore& vstore,
                                          const RawGlyph& composite,
                                          const LoadOptions& opts,
                                          int depth)
{
    ProcessedGlyph pg{};
    pg.is_composite = true;
    pg.is_empty     = false;
    pg.x_min = composite.x_min;
    pg.y_min = composite.y_min;
    pg.x_max = composite.x_max;
    pg.y_max = composite.y_max;

    // Anchor origin for point-matched components: the first parent point a
    // component aligns against is recorded here and reused as the alignment
    // origin by every later point-matched component, so a chain of components
    // shares one consistent reference frame.
    const FPoint* anchor = nullptr;

    for (const auto& comp : composite.components) {
        if (comp.glyph_index >= font.maxp.num_glyphs) continue;

        auto sub_res = load_and_process_glyph(font, hint_ctx, vstore,
                                               comp.glyph_index, opts, depth+1);
        if (!sub_res.ok()) continue;

        ProcessedGlyph sub = std::move(sub_res.value);
        if (sub.is_empty) continue;

        if (comp.flags & kCompArgsAreXYValues) {
            apply_transform(sub, comp);
        } else if (!sub.points.empty()) {
            // Point-matching placement (TrueType §composite glyphs): the
            // component is translated so its matched point coincides with the
            // anchor. The anchor is captured once from the points emitted by an
            // earlier component and then reused for the rest of the chain.
            if (anchor == nullptr &&
                comp.arg1 >= 0 && size_t(comp.arg1) < pg.points.size())
                anchor = &pg.points[size_t(comp.arg1)];

            if (anchor != nullptr) {
                size_t ci = size_t(uint32_t(comp.arg2)) % sub.points.size();
                FUnit dx = FUnit(int32_t(anchor->x) - int32_t(sub.points[ci].x));
                FUnit dy = FUnit(int32_t(anchor->y) - int32_t(sub.points[ci].y));
                for (auto& pt : sub.points) {
                    pt.x = FUnit(int32_t(pt.x) + dx);
                    pt.y = FUnit(int32_t(pt.y) + dy);
                }
            }
        }

        // Offset contour end points by the current point count.
        uint16_t base = uint16_t(pg.points.size());
        for (auto ep : sub.end_pts)
            pg.end_pts.push_back(uint16_t(ep + base));

        pg.points.insert(pg.points.end(), sub.points.begin(), sub.points.end());
        pg.flags.insert(pg.flags.end(), sub.flags.begin(), sub.flags.end());
    }

    pg.hmetrics = get_glyph_hmetrics(font.hmtx, composite.components.empty() ? 0
                                      : composite.components[0].glyph_index);
    return Result<ProcessedGlyph>::success(std::move(pg));
}

Result<ProcessedGlyph> load_and_process_glyph(
    const FontFace& font,
    HintContext& hint_ctx,
    const VariationStore& vstore,
    uint16_t glyph_id,
    const LoadOptions& opts,
    int depth)
{
    if (depth > kMaxCompositeDepth)
        return Result<ProcessedGlyph>::error(Status::OutOfRange);

    // Load raw glyph from loca+glyf.
    auto raw_res = load_glyph(font, glyph_id);
    if (!raw_res.ok()) return Result<ProcessedGlyph>::error(raw_res.status);
    RawGlyph raw = std::move(raw_res.value);

    if (raw.is_empty()) {
        ProcessedGlyph pg{};
        pg.is_empty    = true;
        pg.hmetrics    = get_glyph_hmetrics(font.hmtx, glyph_id);
        return Result<ProcessedGlyph>::success(pg);
    }

    if (raw.is_composite())
        return resolve_composite(font, hint_ctx, vstore, raw, opts, depth);

    // Apply variation deltas if requested.
    if (opts.apply_variation && !vstore.glyph_data.empty() &&
        glyph_id < vstore.glyph_data.size() &&
        !opts.normalized_coords.empty())
    {
        auto deltas = compute_point_deltas(vstore.regions,
                                           vstore.glyph_data[glyph_id],
                                           opts.normalized_coords,
                                           uint16_t(raw.points.size()));
        apply_deltas(raw.points, deltas);
    }

    // Run the hint program if present and requested.
    if (opts.apply_hints && !raw.instructions.empty()) {
        uint16_t n_pts = uint16_t(raw.points.size());
        hint_ctx.zones[1].resize(n_pts);

        // Copy point positions into zone 1.
        for (uint16_t i = 0; i < n_pts; ++i) {
            hint_ctx.zones[1].x_coords[i]  = Fixed16::from_int(raw.points[i].x);
            hint_ctx.zones[1].y_coords[i]  = Fixed16::from_int(raw.points[i].y);
            hint_ctx.zones[1].ox_coords[i] = hint_ctx.zones[1].x_coords[i];
            hint_ctx.zones[1].oy_coords[i] = hint_ctx.zones[1].y_coords[i];
            hint_ctx.zones[1].touched_x[i] = false;
            hint_ctx.zones[1].touched_y[i] = false;
        }
        hint_ctx.num_glyph_pts = n_pts;

        FpgmTable fpgm;
        const TableRecord* fpgm_rec = find_table(font.sfnt, tags::FPGM());
        if (fpgm_rec &&
            fpgm_rec->offset <= font.raw_data.size() &&
            fpgm_rec->length <= font.raw_data.size() - fpgm_rec->offset) {
            fpgm.assign(
                font.raw_data.data() + fpgm_rec->offset,
                font.raw_data.data() + fpgm_rec->offset + fpgm_rec->length);
        }

        execute_hint_program(hint_ctx, raw.instructions.data(),
                             raw.instructions.size(), fpgm);

        // Copy hinted positions back.
        for (uint16_t i = 0; i < n_pts && i < hint_ctx.zones[1].size(); ++i) {
            raw.points[i].x = FUnit(hint_ctx.zones[1].x_coords[i].integer());
            raw.points[i].y = FUnit(hint_ctx.zones[1].y_coords[i].integer());
        }

        // IUP pass for unhinted points.
        if (opts.apply_iup) {
            iup_interpolate(raw, hint_ctx.zones[1].touched_x,
                            hint_ctx.zones[1].touched_y);
        }
    }

    ProcessedGlyph pg{};
    pg.points       = raw.points;
    pg.end_pts      = raw.end_pts_of_contours;
    pg.flags        = raw.flags;
    pg.x_min        = raw.x_min;
    pg.y_min        = raw.y_min;
    pg.x_max        = raw.x_max;
    pg.y_max        = raw.y_max;
    pg.hmetrics     = get_glyph_hmetrics(font.hmtx, glyph_id);
    pg.is_composite = false;
    pg.is_empty     = false;
    return Result<ProcessedGlyph>::success(std::move(pg));
}

Result<ProcessedGlyph> load_glyph_simple(const FontFace& font, uint16_t glyph_id,
                                          const LoadOptions& opts)
{
    HintContext ctx = make_hint_context(font, glyph_id, opts.ppem);
    VariationStore vstore;
    return load_and_process_glyph(font, ctx, vstore, glyph_id, opts);
}

} // namespace fontscope
