# fontscope

fontscope is a C++17 library and command-line tool for inspecting, validating
and fuzzing OpenType/TrueType font files. It parses the SFNT container and the
TrueType tables listed below, checks them for structural problems, and dumps
metadata, metrics and glyph outlines. Table parsers read through a
bounds-checked `ByteReader`, and the library ships with libFuzzer harnesses
for its parsing, hinting, variation and rendering paths.

## Supported tables

| Area               | Tables |
|--------------------|--------|
| Core               | `head`, `maxp`, `hhea`, `hmtx`, `OS/2`, `post` (including v2.0 glyph names), `name` |
| Character mapping  | `cmap` subtable formats 0, 4, 6 and 12 |
| Outlines           | `loca` (short and long), `glyf` (simple and composite glyphs) |
| Kerning            | `kern` subtable formats 0 and 2 |
| Color and bitmaps  | `COLR` v0 layers, `CPAL` palettes, `sbix` bitmap strikes |
| Variations         | `fvar`, `avar`, `gvar` |
| Hinting            | `cvt `, `fpgm`, glyph instructions (partial interpreter, see [Limitations](#limitations)) |

The library also includes a glyph subsetter, an SVG exporter, a scanline
rasterizer, JSON output and a font diff. CFF/CFF2 outlines, `GSUB`/`GPOS`,
`prep` and `cvar` are not parsed.

## Build and test

Requires CMake 3.16+ and a C++17 compiler. The build is warning-free under
`-Wall -Wextra` with GCC 16 and Apple clang 21.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Or with the Makefile: `make && make test`.

## Example

Real output from `fontscope` run on Arial, which ships with macOS (excerpts):

```console
$ ./build/fontscope inspect /System/Library/Fonts/Supplemental/Arial.ttf
Family:       Arial
Subfamily:    Regular
Version:      Version 5.01.2x
SFNT version: TrueType 1.0
Tables:       24
Glyphs:       3381
Units/em:     2048
Bbox:         (-1361, -665)–(4096, 2060)
Ascender:     1854
Descender:    -434
...

$ ./build/fontscope validate /System/Library/Fonts/Supplemental/Arial.ttf
[info] *      no issues found

0 error(s), 0 warning(s)

$ ./build/fontscope dump-glyph /System/Library/Fonts/Supplemental/Arial.ttf U+0041
U+0041 → glyph 36
Glyph 36:
  Type: simple
  Bbox: (-3, 0)–(1369, 1466)
  Contours: 2
  Points:   15
  Instructions: 359 bytes
  Contour 0 (8 pts):
    [0] -3 0 on
    [1] 560 1466 on
    [2] 769 1466 on
    [3] 1369 0 on
...
  AdvanceWidth: 1366  LSB: -3
```

## CLI usage

```
fontscope <command> <font.ttf> [args]

  inspect       <font>              Show font metadata
  tables        <font>              List all font tables
  validate      <font>              Validate table structure
  dump-glyph    <font> <id|U+cp>    Dump glyph outline
  cmap          <font>              List cmap subtables and mappings
  name          <font>              Print all name records
  metrics       <font>              Print font metrics
  advances      <font> [from] [to]  Print per-glyph advance widths
  outline-stats <font>              Count points, contours
  kern-lookup   <font> <L> <R>      Kerning value for glyph pair
  color-glyphs  <font>              List COLR color glyphs
  var-axes      <font>              List variation axes
```

## Library API

```cpp
#include <fontscope/inspect.h>
#include <fontscope/validate.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});

    auto loaded = fontscope::load_font(data.data(), data.size());
    if (!loaded.ok()) {
        std::fprintf(stderr, "error: %s\n", fontscope::status_string(loaded.status));
        return 1;
    }
    const fontscope::FontFace& font = loaded.value;
    fontscope::print_font_info(font);

    // Structural and cross-table checks
    auto report = fontscope::validate_font(font);
    for (const auto& issue : report.issues)
        std::printf("[%s] %s\n", issue.table.c_str(), issue.message.c_str());

    // Outline of the glyph mapped to 'A'
    uint16_t gid = fontscope::codepoint_to_glyph(font, 'A');
    auto glyph = fontscope::load_glyph(font, gid);
    if (glyph.ok())
        for (const auto& pt : glyph.value.points)
            std::printf("%d %d %s\n", pt.x, pt.y, pt.on_curve ? "on" : "off");
}
```

Link against the `fontscope` CMake target (or `build/libfontscope.a` with the
Makefile) and add `include/` to the include path.

## Fuzzing

There are eight [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harnesses in
`fuzz/`:

| Harness                | Exercises |
|------------------------|-----------|
| `font_fuzzer`          | Full load, validation, glyph processing, `gvar` and color glyphs |
| `hint_fuzzer`          | Glyph loading with the TrueType hinting interpreter |
| `variation_fuzzer`     | `gvar` parsing and variation deltas |
| `colr_bitmap_fuzzer`   | `COLR`/`CPAL` color glyph rendering |
| `raster_fuzzer`        | Glyph rasterization |
| `layout_fuzzer`        | `kern` formats 0 and 2 and string layout |
| `bitmap_strike_fuzzer` | `sbix` strikes and bitmap headers |
| `cache_fuzzer`         | Cached glyph-run rendering under LRU eviction |

Seed corpora live in `fuzz/corpus/<harness>/` and a shared dictionary in
`fuzz/font.dict`. Harnesses without their own corpus directory can use
`fuzz/corpus/font_fuzzer/`.

The harnesses need clang with libFuzzer. On Linux the distribution's clang
works. Apple's Xcode clang does not ship libFuzzer, so on macOS use Homebrew
LLVM, for example
`-DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"`. Turn the tests and
CLI off for this build, because the library is compiled with ASan/UBSan here
and they are not linked against the sanitizer runtimes.

```bash
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_FUZZERS=ON -DBUILD_TESTS=OFF -DBUILD_CLI=OFF
cmake --build build-fuzz

# Fuzz for five minutes. New inputs go to the first directory and the
# checked-in seeds are only read.
mkdir -p build-fuzz/corpus/font_fuzzer
./build-fuzz/font_fuzzer build-fuzz/corpus/font_fuzzer fuzz/corpus/font_fuzzer \
    -dict=fuzz/font.dict -max_total_time=300

# Replay the seed corpus once, e.g. as a regression check
./build-fuzz/hint_fuzzer -runs=0 fuzz/corpus/hint_fuzzer

# Reproduce a crash that libFuzzer saved
./build-fuzz/font_fuzzer crash-<sha1>
```

`.clusterfuzzlite/build.sh` builds every harness OSS-Fuzz style (it uses
`$CXX`, `$CXXFLAGS` and `$OUT`) and packages each seed corpus and the
dictionary next to its binary.

## Limitations

- **Hinting interpreter:** function definitions and calls are not supported.
  `FDEF`/`ENDF` bodies are skipped and `CALL`/`LOOPCALL` only pop their
  arguments, so `fpgm` functions are never executed (`execute_hint_program`
  accepts the `fpgm` table but does not run it). Fonts whose hinting depends on
  `fpgm` functions, which is most hinted fonts, are not hinted faithfully.
  Several other instructions, such as `ISECT`, `DELTAP*` and `FLIPPT`, only
  pop their arguments.
- `prep` is not executed and `cvar` is not applied.
- Only TrueType (`glyf`) outlines are supported. CFF/CFF2 fonts are not.
- Layout applies `kern` pairs only, with no `GSUB`/`GPOS` shaping.
- **Subsetter:** `glyf`, `loca`, `hmtx` and `cmap` are rebuilt. The new
  `cmap` is format 4, so codepoints above U+FFFF get no entry. `post` and
  `kern` are copied unchanged, so glyph names and kerning pairs still use the
  original glyph IDs, and `OS/2` is not carried over.

## Project layout

```
include/fontscope/   public headers
src/                 library implementation
tools/               CLI entry point (fontscope)
tests/               unit tests (ctest)
fuzz/                libFuzzer harnesses, seed corpora, dictionary
.clusterfuzzlite/    OSS-Fuzz style build script for the harnesses
docs/                format notes
```

See [docs/opentype_subset.md](docs/opentype_subset.md) for notes on the table
subset this library handles.

## License

MIT, see [LICENSE](LICENSE).
