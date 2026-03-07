#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/errors.h"
#include "fontscope/reader.h"
#include "fontscope/fixed.h"
#include "fontscope/font_types.h"

namespace fontscope {

struct VariationAxis {
    Tag     axis_tag;
    Fixed16 min_value;
    Fixed16 default_value;
    Fixed16 max_value;
    uint16_t flags;
    uint16_t axis_name_id;
};

struct NamedInstance {
    uint16_t              subfamily_name_id;
    uint16_t              flags;
    std::vector<Fixed16>  coordinates;
    uint16_t              post_script_name_id;
};

struct FvarTable {
    uint16_t                  major_version;
    uint16_t                  minor_version;
    std::vector<VariationAxis>  axes;
    std::vector<NamedInstance>  named_instances;
};

Result<FvarTable> parse_fvar(ByteReader& r);

// Normalize a user-space axis value to [-1, 0, 1] range.
F2Dot14 normalize_axis(const VariationAxis& axis, Fixed16 user_value);

} // namespace fontscope
