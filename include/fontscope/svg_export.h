#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "fontscope/glyf.h"
#include "fontscope/inspect.h"
#include "fontscope/pipeline.h"

namespace fontscope {

// Options for SVG export.
struct SvgExportOptions {
    int     viewport_width  = 512;
    int     viewport_height = 512;
    bool    flip_y          = true;    // TrueType Y-up → SVG Y-down
    bool    add_grid        = false;   // draw UPM grid lines
    bool    add_metrics     = false;   // draw ascender/descender lines
    bool    add_bbox        = false;   // draw glyph bounding box
    bool    pretty          = true;    // indent / newlines in output
    std::string fill_color  = "#000000";
    std::string stroke_color = "none";
    double  stroke_width    = 0.0;
    double  opacity         = 1.0;
};

// Convert a RawGlyph (in font units) to an SVG <path d="…"> string.
// Returns the d= attribute value only (no surrounding element).
std::string glyph_to_svg_path(const RawGlyph& g, const SvgExportOptions& opts);

// Build a complete SVG document containing one glyph centered in the viewport.
std::string glyph_to_svg(const FontFace& font, const RawGlyph& g,
                          uint16_t glyph_id, const SvgExportOptions& opts);

// Build an SVG document with multiple glyphs laid out horizontally.
std::string glyphs_to_svg_sheet(
    const FontFace&              font,
    const std::vector<uint16_t>& glyph_ids,
    uint16_t                     ppem,
    const SvgExportOptions&      opts);

// Export the full font's glyph set to a multi-page SVG (one <g> per glyph).
// Only non-empty glyphs are included.
std::string font_to_svg(const FontFace& font, const SvgExportOptions& opts);

// Decompose a single contour (start..end inclusive) to SVG path commands.
// Quadratic off-curves are expanded to cubic for maximum compatibility.
std::string contour_to_svg_commands(
    const std::vector<FPoint>& pts,
    uint16_t start, uint16_t end, bool flip_y, double scale,
    double tx, double ty);

// Convert a single FPoint coordinate with optional Y-flip and scale.
double svg_x(FUnit x, double scale, double tx);
double svg_y(FUnit y, double scale, double ty, bool flip, int viewport_height);

} // namespace fontscope
