cmake_minimum_required(VERSION 3.16)
project(fontscope LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS     "Build unit tests"     ON)
option(BUILD_CLI       "Build CLI tool"       ON)
option(BUILD_FUZZERS   "Build fuzz harness"   OFF)
option(ENABLE_SANITIZERS "Enable ASan/UBSan"  OFF)

if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(LIB_SRCS
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

add_library(fontscope STATIC ${LIB_SRCS})
target_include_directories(fontscope PUBLIC include)
target_compile_options(fontscope PRIVATE -Wall -Wextra)

if(BUILD_CLI)
    add_executable(fontscope_cli tools/fontscope.cpp)
    target_link_libraries(fontscope_cli fontscope)
    set_target_properties(fontscope_cli PROPERTIES OUTPUT_NAME fontscope)
endif()

if(BUILD_TESTS)
    enable_testing()

    function(add_fs_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} fontscope)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_fs_test(test_reader)
    add_fs_test(test_fixed)
    add_fs_test(test_sfnt)
    add_fs_test(test_glyf)
    add_fs_test(test_hint_vm)
    add_fs_test(test_colr)
    add_fs_test(test_variation)
    add_fs_test(test_validate)
    add_fs_test(test_matrix)
    add_fs_test(test_json)
    add_fs_test(test_outline_stats)
    add_fs_test(test_layout)
    add_fs_test(test_svg)
    add_fs_test(test_subsetter)
    add_fs_test(test_raster)
    add_fs_test(test_glyph_loader)
endif()

if(BUILD_FUZZERS)
    # The harnesses link the static library, so the library itself must be
    # instrumented for the sanitizers to see into table parsing and glyph
    # processing — not just the harness translation units.
    target_compile_options(fontscope PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)

    set(FUZZ_TARGETS
        font_fuzzer
        hint_fuzzer
        variation_fuzzer
        colr_bitmap_fuzzer
        raster_fuzzer
        layout_fuzzer
        bitmap_strike_fuzzer
        cache_fuzzer
    )
    foreach(target IN LISTS FUZZ_TARGETS)
        add_executable(${target} fuzz/${target}.cc)
        target_link_libraries(${target} fontscope)
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
endif()
