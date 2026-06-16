#pragma once
#include <cstdint>
#include <string>

namespace fontscope {

// Library version information.
constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 9;
constexpr int kVersionPatch = 0;

// Return version string like "0.9.0".
std::string version_string();

// Return a short build summary: version + OpenType feature set.
std::string build_info();

// Capabilities that can be queried at runtime.
bool has_colr_support();   // always true — COLR/CPAL parsed
bool has_gvar_support();   // always true — gvar variation parsed
bool has_hinting_support();// always true — TrueType hint VM present
bool has_aa_raster();      // always true — anti-aliased rasterizer linked

} // namespace fontscope
