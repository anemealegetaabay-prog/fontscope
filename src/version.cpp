#include "fontscope/version.h"
#include <cstdio>

namespace fontscope {

std::string version_string() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d.%d.%d",
             kVersionMajor, kVersionMinor, kVersionPatch);
    return buf;
}

std::string build_info() {
    return std::string("fontscope ") + version_string()
         + " [OpenType/TrueType; COLR/CPAL; gvar; hint-VM; AA-raster]";
}

bool has_colr_support()    { return true; }
bool has_gvar_support()    { return true; }
bool has_hinting_support() { return true; }
bool has_aa_raster()       { return true; }

} // namespace fontscope
