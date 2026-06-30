#!/bin/bash -eu

# fontscope — ClusterFuzzLite build script.
#
# Compiles all library sources and links every fuzz harness with
# -fsanitize=fuzzer,address,undefined. The $OUT directory is provided
# by the ClusterFuzzLite runner.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

CXX="${CXX:-clang++}"
CXXFLAGS="${CXXFLAGS:- -O1 -g}"
CXXFLAGS="$CXXFLAGS -std=c++17 -Iinclude"
CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined,fuzzer-no-link"
CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer"

SRCS=(
    src/avar.cpp
    src/bitmap.cpp
    src/cache.cpp
    src/cmap.cpp
    src/colr.cpp
    src/cpal.cpp
    src/deltas.cpp
    src/fixed.cpp
    src/font_diff.cpp
    src/fvar.cpp
    src/glyf.cpp
    src/glyph_loader.cpp
    src/hb_compat.cpp
    src/hint_vm.cpp
    src/inspect.cpp
    src/inspect_json.cpp
    src/kern.cpp
    src/layout.cpp
    src/loca.cpp
    src/matrix.cpp
    src/metrics.cpp
    src/name_table.cpp
    src/outline_stats.cpp
    src/pipeline.cpp
    src/raster.cpp
    src/raster_aa.cpp
    src/sbix.cpp
    src/sfnt.cpp
    src/shaper.cpp
    src/subsetter.cpp
    src/svg_export.cpp
    src/tables.cpp
    src/text_render.cpp
    src/unicode_blocks.cpp
    src/validate.cpp
    src/variation.cpp
    src/version.cpp
)

FUZZERS=(
    font_fuzzer
    hint_fuzzer
    variation_fuzzer
    colr_bitmap_fuzzer
    raster_fuzzer
    layout_fuzzer
    bitmap_strike_fuzzer
    cache_fuzzer
)

# Compile library objects.
OBJ_FILES=()
for f in "${SRCS[@]}"; do
    obj="$OUT/$(basename "$f" .cpp).o"
    $CXX $CXXFLAGS -c "$f" -o "$obj"
    OBJ_FILES+=("$obj")
done

# Build fuzz harnesses.
for fuzzer in "${FUZZERS[@]}"; do
    $CXX $CXXFLAGS -fsanitize=fuzzer \
        "fuzz/${fuzzer}.cc" \
        "${OBJ_FILES[@]}" \
        -o "$OUT/${fuzzer}"

    corpus_dir="fuzz/corpus/${fuzzer}"
    if [ -d "$corpus_dir" ] && [ "$(ls -A "$corpus_dir" 2>/dev/null)" ]; then
        zip -j "$OUT/${fuzzer}_seed_corpus.zip" "$corpus_dir"/*
    fi
done

# Shared dictionary for all harnesses.
if [ -f fuzz/font.dict ]; then
    cp fuzz/font.dict "$OUT/font_fuzzer.dict"
    for fuzzer in "${FUZZERS[@]}"; do
        cp fuzz/font.dict "$OUT/${fuzzer}.dict"
    done
fi
