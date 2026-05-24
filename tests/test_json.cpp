#include "fontscope/inspect_json.h"
#include "fontscope/glyf.h"
#include "fontscope/outline_stats.h"
#include <cstdio>
#include <cassert>
#include <cstring>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

static bool has(const std::string& s, const char* sub) {
    return s.find(sub) != std::string::npos;
}

static void test_json_escape() {
    CHECK(json_escape("hello") == "hello");
    CHECK(json_escape("a\"b") == "a\\\"b");
    CHECK(json_escape("a\\b") == "a\\\\b");
    CHECK(json_escape("a\nb") == "a\\nb");
    CHECK(json_escape("a\tb") == "a\\tb");
    // Control char U+0001.
    std::string s; s += char(1);
    std::string esc = json_escape(s);
    CHECK(esc == "\\u0001");
}

static void test_json_writer_object() {
    JsonWriter w;
    w.begin_object();
    w.kv_string("name", "fontscope");
    w.kv_int("version", 1);
    w.kv_uint("size", 65536u);
    w.kv_bool("valid", true);
    w.kv_double("scale", 1.5, 2);
    w.end_object();

    std::string s = w.str();
    CHECK(s[0] == '{');
    CHECK(s.back() == '}');
    CHECK(has(s, "\"name\":\"fontscope\""));
    CHECK(has(s, "\"version\":1"));
    CHECK(has(s, "\"size\":65536"));
    CHECK(has(s, "\"valid\":true"));
    CHECK(has(s, "\"scale\":1.50"));
}

static void test_json_writer_array() {
    JsonWriter w;
    w.begin_array();
    w.value_int(1);
    w.value_int(2);
    w.value_int(3);
    w.end_array();

    std::string s = w.str();
    CHECK(s == "[1,2,3]");
}

static void test_json_writer_nested() {
    JsonWriter w;
    w.begin_object();
    w.key("items");
    w.begin_array();
    w.begin_object();
    w.kv_string("a", "b");
    w.end_object();
    w.begin_object();
    w.kv_int("x", 42);
    w.end_object();
    w.end_array();
    w.end_object();

    std::string s = w.str();
    CHECK(s == "{\"items\":[{\"a\":\"b\"},{\"x\":42}]}");
}

static void test_json_writer_null() {
    JsonWriter w;
    w.begin_object();
    w.key("n");
    w.value_null();
    w.end_object();
    CHECK(w.str() == "{\"n\":null}");
}

static void test_json_writer_false() {
    JsonWriter w;
    w.begin_object();
    w.kv_bool("f", false);
    w.end_object();
    CHECK(w.str() == "{\"f\":false}");
}

static void test_glyph_to_json_empty() {
    RawGlyph g{};
    // Empty glyph: num_contours == 0, no points.
    g.x_min = g.y_min = g.x_max = g.y_max = 0;
    std::string s = glyph_to_json(g, 0);
    CHECK(has(s, "\"glyphId\":0"));
    // Either "simple" or "empty" — empty glyph has no end_pts.
    CHECK(has(s, "\"type\""));
}

static void test_glyph_to_json_simple() {
    RawGlyph g{};
    g.number_of_contours = 1;
    g.x_min = 0; g.y_min = 0; g.x_max = 100; g.y_max = 200;
    g.end_pts_of_contours = {2};
    g.points = {{0, 0, true}, {50, 100, false}, {100, 0, true}};

    std::string s = glyph_to_json(g, 7);
    CHECK(has(s, "\"glyphId\":7"));
    CHECK(has(s, "\"type\":\"simple\""));
    CHECK(has(s, "\"numContours\":1"));
    CHECK(has(s, "\"numPoints\":3"));
    CHECK(has(s, "contours"));
    CHECK(has(s, "\"onCurve\":true"));
    CHECK(has(s, "\"onCurve\":false"));
}

static void test_outline_summary_to_json() {
    OutlineSummary s{};
    s.total_glyphs    = 10;
    s.simple_glyphs   = 7;
    s.composite_glyphs = 1;
    s.empty_glyphs    = 2;
    s.total_points    = 300;
    s.total_contours  = 50;
    s.max_points      = 80;
    s.avg_points_per_glyph = 42.857;

    std::string j = outline_summary_to_json(s);
    CHECK(has(j, "\"totalGlyphs\":10"));
    CHECK(has(j, "\"simpleGlyphs\":7"));
    CHECK(has(j, "\"compositeGlyphs\":1"));
    CHECK(has(j, "\"emptyGlyphs\":2"));
    CHECK(has(j, "\"totalPoints\":300"));
    CHECK(has(j, "\"maxPoints\":80"));
    CHECK(has(j, "\"avgPointsPerGlyph\""));
}

static void test_validation_report_to_json() {
    ValidationReport report{};
    report.error_count   = 1;
    report.warning_count = 2;
    report.issues.push_back({IssueSeverity::Error,   "head", "bad magic"});
    report.issues.push_back({IssueSeverity::Warning, "hhea", "mismatch"});
    report.issues.push_back({IssueSeverity::Info,    "post", "ok"});

    std::string j = validation_report_to_json(report);
    CHECK(has(j, "\"errors\":1"));
    CHECK(has(j, "\"warnings\":2"));
    CHECK(has(j, "\"severity\":\"error\""));
    CHECK(has(j, "\"severity\":\"warning\""));
    CHECK(has(j, "\"severity\":\"info\""));
    CHECK(has(j, "\"table\":\"head\""));
    CHECK(has(j, "\"message\":\"bad magic\""));
}

int main() {
    test_json_escape();
    test_json_writer_object();
    test_json_writer_array();
    test_json_writer_nested();
    test_json_writer_null();
    test_json_writer_false();
    test_glyph_to_json_empty();
    test_glyph_to_json_simple();
    test_outline_summary_to_json();
    test_validation_report_to_json();

    if (failures) {
        fprintf(stderr, "test_json: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_json: all passed\n");
    return 0;
}
