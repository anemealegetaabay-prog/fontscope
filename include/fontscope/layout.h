#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "fontscope/inspect.h"
#include "fontscope/pipeline.h"

namespace fontscope {

// Direction of text flow.
enum class TextDirection { LTR, RTL };

// A single positioned glyph run item.
struct GlyphItem {
    uint16_t glyph_id;
    uint32_t cluster;     // Unicode cluster index
    int32_t  x_advance;  // advance in pixels (scaled)
    int32_t  y_advance;
    int32_t  x_offset;   // offset from pen position
    int32_t  y_offset;
};

// A text run: a sequence of characters with uniform direction and style.
struct TextRun {
    std::vector<uint32_t> codepoints;
    TextDirection         direction{TextDirection::LTR};
    uint16_t              ppem{16};
    uint16_t              palette_id{0};
};

// Layout result for a single line.
struct LineLayout {
    std::vector<GlyphItem> items;
    int32_t  total_width;
    int32_t  ascender;
    int32_t  descender;
    int32_t  line_gap;
    int32_t  baseline_y;

    int32_t line_height() const { return ascender - descender + line_gap; }
};

// Multi-line layout result.
struct TextLayout {
    std::vector<LineLayout> lines;
    int32_t max_width;
    int32_t total_height;
    int32_t line_height;
};

// Shape a text run to a sequence of positioned GlyphItems.
// Applies cmap lookup, kern pair adjustments, and advance accumulation.
LineLayout shape_text_run(const FontFace& font, const TextRun& run);

// Break a text run into multiple lines at the given pixel width.
TextLayout layout_text(const FontFace& font, const TextRun& run, int32_t max_width);

// Convert a UTF-8 string to a vector of Unicode code points.
std::vector<uint32_t> utf8_to_codepoints(const std::string& utf8);

// Convert a vector of Unicode code points back to UTF-8.
std::string codepoints_to_utf8(const std::vector<uint32_t>& codepoints);

// Compute the pixel width of a text run without rasterizing.
int32_t measure_text_run(const FontFace& font, const TextRun& run);

// Find the logical cursor position (byte offset) for a given pixel x in a run.
int32_t hit_test(const LineLayout& layout, int32_t pixel_x);

// Return the visual bounding box of a shaped line.
struct LineBBox {
    int32_t x_min, y_min, x_max, y_max;
};
LineBBox line_bbox(const LineLayout& layout, const FontFace& font);

// Apply simple bidirectional reordering (LTR only for now; RTL stub).
void reorder_bidi(std::vector<GlyphItem>& items, TextDirection dir);

// Compute glyph cluster advance widths for a shaped line.
std::vector<int32_t> cluster_advances(const LineLayout& layout);

} // namespace fontscope
