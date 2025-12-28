# fontscope

A C++ library and command-line tool for inspecting, validating, and analyzing
OpenType and TrueType font files.

## Features

- Parse SFNT container headers, table directories, and checksums
- Inspect font metadata: family name, units per em, glyph count, OS/2 metrics
- Dump glyph outlines from `glyf` tables — simple and composite glyphs
- Enumerate cmap subtables and resolve Unicode code points to glyph IDs
- Validate table offsets, length consistency, and checksum integrity
- Analyze TrueType hinting programs: decode `fpgm`/`prep`/`cvt ` and trace instructions
- Parse COLR/CPAL color glyph layers and sub-byte bitmap blending
- Apply `gvar`/`cvar` variation deltas to glyph outlines
- Report `hhea`/`hmtx` advance widths and `kern` pair adjustments
- Query `name` table records including family, style, copyright, and license strings

## Quick Start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or with the provided Makefile:

```bash
make
```

## CLI Usage

```
fontscope inspect   <font.ttf>
fontscope tables    <font.ttf>
fontscope validate  <font.ttf>
fontscope dump-glyph <font.ttf> <glyph-id|U+codepoint>
fontscope cmap      <font.ttf>
fontscope name      <font.ttf>
fontscope metrics   <font.ttf>
```

### Examples

```bash
# Show font metadata
fontscope inspect /usr/share/fonts/NotoSans-Regular.ttf

# List all font tables with their sizes and checksums
fontscope tables /usr/share/fonts/NotoSans-Regular.ttf

# Dump the contour outline for the letter 'A'
fontscope dump-glyph /usr/share/fonts/NotoSans-Regular.ttf U+0041

# Validate table directory, checksums, and cross-table consistency
fontscope validate /usr/share/fonts/NotoSans-Regular.ttf

# List cmap subtables and a sample of Unicode mappings
fontscope cmap /usr/share/fonts/NotoSans-Regular.ttf

# Print all name records
fontscope name /usr/share/fonts/NotoSans-Regular.ttf

# Print advance widths, ascender, descender, linegap
fontscope metrics /usr/share/fonts/NotoSans-Regular.ttf
```

## Library API

```cpp
#include <fontscope/inspect.h>
#include <fontscope/validate.h>
#include <fontscope/glyf.h>

// Load and inspect a font
auto result = fontscope::load_font(data, size);
if (!result.ok()) { /* handle error */ }

auto& font = result.value();
fontscope::print_font_info(font);

// Validate
auto report = fontscope::validate_font(font);
for (const auto& issue : report.issues)
    printf("%s\n", issue.message.c_str());

// Dump a glyph outline
auto glyph = fontscope::load_glyph(font, glyph_id);
for (const auto& pt : glyph.points)
    printf("  %.1f %.1f %s\n", pt.x, pt.y, pt.on_curve ? "on" : "off");
```

## Building with Sanitizers

```bash
make sanitize
# or
cmake -B build -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Project Layout

```
include/fontscope/   — public headers
src/                 — library implementation
tools/               — CLI entry point
tests/               — unit tests
fuzz/                — libFuzzer harness and seed corpus
docs/                — format notes
```

## Documentation

See [docs/opentype_subset.md](docs/opentype_subset.md) for notes on the OpenType
table subset this library handles.

## License

MIT
