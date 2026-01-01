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
endif()

if(BUILD_FUZZERS)
    add_executable(font_fuzzer fuzz/font_fuzzer.cc)
    target_link_libraries(font_fuzzer fontscope)
    target_compile_options(font_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(font_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
