#include "fontscope/fvar.h"
#include <algorithm>

namespace fontscope {

Result<FvarTable> parse_fvar(ByteReader& r) {
    FvarTable fvar{};
    fvar.major_version = r.read_u16_be();
    fvar.minor_version = r.read_u16_be();
    uint16_t axes_array_offset    = r.read_u16_be();
    r.skip(2);  // reserved
    uint16_t axis_count           = r.read_u16_be();
    uint16_t axis_size            = r.read_u16_be();
    uint16_t instance_count       = r.read_u16_be();
    uint16_t instance_size        = r.read_u16_be();
    if (!r.ok()) return Result<FvarTable>::error(Status::TruncatedInput);

    // Read axes.
    ByteReader ar = r.sub_reader(axes_array_offset,
                                  r.size() - axes_array_offset);
    fvar.axes.reserve(axis_count);
    for (uint16_t i = 0; i < axis_count; ++i) {
        size_t axis_start = ar.pos();
        VariationAxis ax{};
        ax.axis_tag.value  = ar.read_u32_be();
        ax.min_value       = Fixed16::from_raw(ar.read_i32_be());
        ax.default_value   = Fixed16::from_raw(ar.read_i32_be());
        ax.max_value       = Fixed16::from_raw(ar.read_i32_be());
        ax.flags           = ar.read_u16_be();
        ax.axis_name_id    = ar.read_u16_be();
        if (!ar.ok()) break;
        // Skip remainder if axis_size > 20 (forward compatibility).
        if (axis_size > 20)
            ar.seek(axis_start + axis_size);
        fvar.axes.push_back(ax);
    }

    // Read named instances.
    size_t inst_offset = axes_array_offset
                       + uint32_t(axis_count) * axis_size;
    ByteReader ir = r.sub_reader(inst_offset, r.size() - inst_offset);
    fvar.named_instances.reserve(instance_count);
    for (uint16_t i = 0; i < instance_count; ++i) {
        size_t inst_start = ir.pos();
        NamedInstance ni{};
        ni.subfamily_name_id   = ir.read_u16_be();
        ni.flags               = ir.read_u16_be();
        ni.coordinates.resize(axis_count);
        for (auto& coord : ni.coordinates)
            coord = Fixed16::from_raw(ir.read_i32_be());
        if (!ir.ok()) break;
        // Optional postScriptNameID.
        if (instance_size >= uint16_t(4 + axis_count * 4 + 2))
            ni.post_script_name_id = ir.read_u16_be();
        if (instance_size > 0)
            ir.seek(inst_start + instance_size);
        fvar.named_instances.push_back(std::move(ni));
    }

    return Result<FvarTable>::success(std::move(fvar));
}

F2Dot14 normalize_axis(const VariationAxis& axis, Fixed16 user_value) {
    if (user_value <= axis.min_value) return F2Dot14::from_f64(-1.0);
    if (user_value >= axis.max_value) return F2Dot14::from_f64(1.0);
    if (user_value == axis.default_value) return F2Dot14::from_raw(0);

    double val = user_value.to_f64();
    double def = axis.default_value.to_f64();
    double mn  = axis.min_value.to_f64();
    double mx  = axis.max_value.to_f64();

    double norm = (val < def)
                ? -(def - val) / (def - mn)
                :  (val - def) / (mx - def);

    return F2Dot14::from_f64(norm);
}

} // namespace fontscope
