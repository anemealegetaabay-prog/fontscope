#!/bin/bash -eu

# fontscope — ClusterFuzzLite build script.
#
# Compiles all library sources and links the fuzz harness with
# -fsanitize=fuzzer,address,undefined. The $OUT directory is provided
# by the ClusterFuzzLite runner.

SRC_DIR="$(dirname "$0")/.."
cd "$SRC_DIR"

CXX="${CXX:-clang++}"
CXXFLAGS="${CXXFLAGS:- -O1 -g}"
CXXFLAGS="$CXXFLAGS -std=c++17 -Iinclude"
CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined,fuzzer-no-link"
CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer"

SRCS=(
    src/fixed.cpp
    src/sfnt.cpp
    src/tables.cpp
    src/cmap.cpp
    src/loca.cpp
    src/glyf.cpp
    src/hint_vm.cpp
    src/colr.cpp
    src/cpal.cpp
    src/bitmap.cpp
    src/variation.cpp
    src/deltas.cpp
    src/name_table.cpp
    src/fvar.cpp
    src/avar.cpp
    src/kern.cpp
    src/inspect.cpp
    src/validate.cpp
    src/metrics.cpp
    src/raster.cpp
    src/raster_aa.cpp
    src/glyph_loader.cpp
    src/pipeline.cpp
)

# Compile library objects.
OBJ_FILES=()
for f in "${SRCS[@]}"; do
    obj="$OUT/$(basename "$f" .cpp).o"
    $CXX $CXXFLAGS -c "$f" -o "$obj"
    OBJ_FILES+=("$obj")
done

# Build fuzz harness.
$CXX $CXXFLAGS -fsanitize=fuzzer \
    fuzz/font_fuzzer.cc \
    "${OBJ_FILES[@]}" \
    -o "$OUT/font_fuzzer"

# Package seed corpus.
if [ -d fuzz/corpus/font_fuzzer ] && \
   [ "$(ls -A fuzz/corpus/font_fuzzer 2>/dev/null)" ]; then
    zip -j "$OUT/font_fuzzer_seed_corpus.zip" fuzz/corpus/font_fuzzer/*
fi
