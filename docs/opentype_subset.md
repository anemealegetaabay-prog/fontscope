# OpenType/TrueType Table Subset

fontscope parses and processes the following OpenType/TrueType tables.
Tables not listed here are ignored; the library returns an error if a
required table is absent.

## Required Tables

| Tag   | Name                    | Notes |
|-------|-------------------------|-------|
| head  | Font header             | Provides UPM, bounding box, `indexToLocFormat` |
| maxp  | Maximum profile         | Glyph count, zone and point limits for hinting |
| hhea  | Horizontal header       | Ascender, descender, line gap, `numberOfHMetrics` |
| hmtx  | Horizontal metrics      | Advance widths and left side bearings per glyph |
| cmap  | Character map           | Format 0, 4, 6, 12 subtables; platform 0/3 preferred |
| loca  | Index to location       | Short (÷2) and long (absolute) offset arrays |
| glyf  | Glyph data              | Simple contours and composite references |
| post  | PostScript              | `isFixedPitch` and underline metrics |
| name  | Naming table            | Format 0 and 1; UTF-16BE strings decoded to UTF-8 |
| OS/2  | OS/2 and Windows        | Weight class, Unicode ranges, win metrics |

## Optional Tables

| Tag   | Name                          | Notes |
|-------|-------------------------------|-------|
| kern  | Kerning                       | Format 0 pair-kerning only |
| COLR  | Color glyph layers (v0)       | BaseGlyphRecord + LayerRecord arrays |
| CPAL  | Color palette                 | v0 palette entries (BGRA) |
| fvar  | Font variations               | Axis records and named instances |
| avar  | Axis variations               | Piecewise-linear segment mapping |
| gvar  | Glyph variations              | Tuple variation stores per glyph |
| cvt   | Control value table           | Integer CVT array for hinting |
| fpgm  | Font program                  | Executed once at font load |
| prep  | Pre-program (CVT program)     | Executed at each size change |

## Parsing Constraints

- All multi-byte integers are big-endian (OpenType standard).
- `ByteReader` enforces bounds on every access; reads past the sub-reader
  window return a parse error rather than undefined behavior from the valid
  code paths.
- Composite glyph recursion is limited to 8 levels to prevent stack exhaustion.
- Hinting VM stack depth is capped at the value in `maxp.maxStackElements`.
- The variation engine accepts up to 64 axes and 4096 tuple variations per glyph.

## Table Ordering and Checksum

fontscope validates the SFNT table directory on load:

1. Verifies the `sfVersion` magic (0x00010000 for TrueType, 0x4F54544F for CFF).
2. Checks that each table's stored offset and length fall within the file.
3. Recomputes each table's checksum and compares against the directory entry.
4. Verifies the `head.checkSumAdjustment` field (0xB1B0AFBA − whole-file checksum).

Checksum mismatches are reported as warnings rather than fatal errors to
accommodate fonts produced by tools that do not update all checksums after
editing.

## Subsetting

The `subset_font()` function emits a structurally valid SFNT containing:

- Remapped `loca` and `glyf` using a contiguous new GID space.
- Rebuilt `hmtx` with only the retained glyphs.
- Rebuilt `cmap` (format 4) mapping only the retained codepoints.
- Correct SFNT table directory with updated offsets and checksums.
- Transitive closure of composite components (collected via `collect_component_gids()`).

Tables not understood by the subsetter (e.g., `GDEF`, `GSUB`, `GPOS`, `CFF `) are
dropped from the output. Variable font tables (`fvar`, `avar`, `gvar`) are also
dropped; subsetting a variable font collapses it to a static instance at default
coordinates.
