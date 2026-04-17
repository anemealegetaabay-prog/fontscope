#include "fontscope/inspect.h"
#include "fontscope/validate.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace fontscope;

// Build a minimal valid font in memory for validation tests.
static std::vector<uint8_t> make_minimal_font() {
    // We can't easily build a fully correct TTF here, so we test validation
    // logic on a FontFace that we construct directly.
    return {};
}

static void test_head_magic_check() {
    FontFace f{};
    f.head.magic_number    = 0xDEADBEEFu;  // wrong
    f.head.units_per_em    = 1000;
    f.head.index_to_loc_format = 0;
    f.maxp.num_glyphs      = 100;
    f.hhea.number_of_h_metrics = 100;

    auto report = validate_font(f);
    bool found = false;
    for (const auto& issue : report.issues)
        if (issue.message.find("magic") != std::string::npos)
            found = true;
    assert(found);
    assert(report.error_count > 0);
}

static void test_units_per_em_range() {
    FontFace f{};
    f.head.magic_number    = 0x5F0F3CF5u;
    f.head.units_per_em    = 10;  // too small
    f.head.index_to_loc_format = 0;
    f.maxp.num_glyphs      = 1;
    f.hhea.number_of_h_metrics = 1;

    auto report = validate_font(f);
    bool found = false;
    for (const auto& issue : report.issues)
        if (issue.message.find("unitsPerEm") != std::string::npos)
            found = true;
    assert(found);
}

static void test_hhea_metrics_consistency() {
    FontFace f{};
    f.head.magic_number    = 0x5F0F3CF5u;
    f.head.units_per_em    = 1000;
    f.head.index_to_loc_format = 0;
    f.maxp.num_glyphs      = 100;
    f.hhea.number_of_h_metrics = 200;  // > num_glyphs

    auto report = validate_font(f);
    bool found = false;
    for (const auto& issue : report.issues)
        if (issue.message.find("numberOfHMetrics") != std::string::npos)
            found = true;
    assert(found);
}

static void test_no_issues() {
    FontFace f{};
    f.head.magic_number    = 0x5F0F3CF5u;
    f.head.units_per_em    = 1000;
    f.head.index_to_loc_format = 0;
    f.head.x_min = -10; f.head.x_max = 100;
    f.head.y_min = -10; f.head.y_max = 800;
    f.maxp.num_glyphs      = 5;
    f.hhea.number_of_h_metrics = 5;

    auto report = validate_font(f);
    // Should find "no issues found" info record.
    bool found_ok = false;
    for (const auto& issue : report.issues)
        if (issue.severity == IssueSeverity::Info) found_ok = true;
    assert(found_ok);
    assert(report.error_count == 0);
}

static void test_bbox_inconsistency() {
    FontFace f{};
    f.head.magic_number    = 0x5F0F3CF5u;
    f.head.units_per_em    = 1000;
    f.head.index_to_loc_format = 0;
    f.head.x_min = 100; f.head.x_max = 50;  // min > max
    f.maxp.num_glyphs      = 5;
    f.hhea.number_of_h_metrics = 5;

    auto report = validate_font(f);
    bool found = false;
    for (const auto& issue : report.issues)
        if (issue.severity == IssueSeverity::Warning &&
            issue.message.find("bounding box") != std::string::npos)
            found = true;
    assert(found);
}

int main() {
    test_head_magic_check();
    test_units_per_em_range();
    test_hhea_metrics_consistency();
    test_no_issues();
    test_bbox_inconsistency();
    puts("test_validate: all passed");
    return 0;
}
