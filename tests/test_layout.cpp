#include "fontscope/layout.h"
#include "fontscope/hb_compat.h"
#include "fontscope/shaper.h"
#include <cstdio>
#include <cassert>
#include <cstring>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

//

static void test_utf8_ascii() {
    std::vector<uint32_t> cps = utf8_to_codepoints("hello");
    CHECK(cps.size() == 5);
    CHECK(cps[0] == 'h');
    CHECK(cps[4] == 'o');
    CHECK(codepoints_to_utf8(cps) == "hello");
}

static void test_utf8_two_byte() {
    // U+00E9 = é, encoded as 0xC3 0xA9.
    std::string s; s += char(0xC3); s += char(0xA9);
    auto cps = utf8_to_codepoints(s);
    CHECK(cps.size() == 1);
    CHECK(cps[0] == 0x00E9u);
    CHECK(codepoints_to_utf8(cps) == s);
}

static void test_utf8_three_byte() {
    // U+4E2D = 中, encoded as 0xE4 0xB8 0xAD.
    std::string s; s += char(0xE4); s += char(0xB8); s += char(0xAD);
    auto cps = utf8_to_codepoints(s);
    CHECK(cps.size() == 1);
    CHECK(cps[0] == 0x4E2Du);
    CHECK(codepoints_to_utf8(cps) == s);
}

static void test_utf8_four_byte() {
    // U+1F600 = emoji, encoded as 0xF0 0x9F 0x98 0x80.
    std::string s;
    s += char(0xF0); s += char(0x9F); s += char(0x98); s += char(0x80);
    auto cps = utf8_to_codepoints(s);
    CHECK(cps.size() == 1);
    CHECK(cps[0] == 0x1F600u);
    CHECK(codepoints_to_utf8(cps) == s);
}

static void test_utf8_mixed() {
    // "aé中" = a(1) + é(2) + 中(3) = 6 bytes, 3 codepoints.
    std::string s = "a";
    s += char(0xC3); s += char(0xA9);
    s += char(0xE4); s += char(0xB8); s += char(0xAD);
    auto cps = utf8_to_codepoints(s);
    CHECK(cps.size() == 3);
    CHECK(cps[0] == 'a');
    CHECK(cps[1] == 0x00E9u);
    CHECK(cps[2] == 0x4E2Du);
    CHECK(codepoints_to_utf8(cps) == s);
}

static void test_utf8_roundtrip_empty() {
    auto cps = utf8_to_codepoints("");
    CHECK(cps.empty());
    CHECK(codepoints_to_utf8(cps).empty());
}

//

static void test_tag_to_str() {
    uint32_t tag = str_to_tag("kern");
    CHECK(tag_to_str(tag) == "kern");
    CHECK(tag_to_str(str_to_tag("liga")) == "liga");
    CHECK(tag_to_str(str_to_tag("DFLT")) == "DFLT");
}

static void test_feature_on_by_default() {
    CHECK(feature_on_by_default(str_to_tag("kern")) == true);
    CHECK(feature_on_by_default(str_to_tag("liga")) == true);
    CHECK(feature_on_by_default(str_to_tag("mark")) == true);
    CHECK(feature_on_by_default(str_to_tag("smcp")) == false);
    CHECK(feature_on_by_default(str_to_tag("dlig")) == false);
    CHECK(feature_on_by_default(str_to_tag("zero")) == false);
}

static void test_find_feature_info() {
    const OtFeatureInfo* f = find_feature_info(str_to_tag("liga"));
    CHECK(f != nullptr);
    CHECK(std::strcmp(f->mnemonic, "liga") == 0);
    CHECK(f->on_by_default == true);

    const OtFeatureInfo* g = find_feature_info("smcp");
    CHECK(g != nullptr);
    CHECK(g->on_by_default == false);

    CHECK(find_feature_info(str_to_tag("ZZZZ")) == nullptr);
}

static void test_parse_feature_string() {
    auto features = parse_feature_string("+kern,+liga,-smcp");
    CHECK(features.size() == 3);
    CHECK(features[0].first == str_to_tag("kern") && features[0].second == true);
    CHECK(features[1].first == str_to_tag("liga") && features[1].second == true);
    CHECK(features[2].first == str_to_tag("smcp") && features[2].second == false);
}

static void test_format_feature_string() {
    std::vector<std::pair<uint32_t,bool>> features = {
        {str_to_tag("kern"), true},
        {str_to_tag("liga"), true},
        {str_to_tag("smcp"), false},
    };
    std::string s = format_feature_string(features);
    CHECK(s == "+kern,+liga,-smcp");
}

static void test_unicode_category() {
    CHECK(unicode_category_approx('A') == UnicodeCategory::UppercaseLetter);
    CHECK(unicode_category_approx('a') == UnicodeCategory::LowercaseLetter);
    CHECK(unicode_category_approx('0') == UnicodeCategory::DecimalNumber);
    CHECK(unicode_category_approx(' ') == UnicodeCategory::SpaceSeparator);
    CHECK(unicode_category_approx('.') == UnicodeCategory::OtherPunct);
    CHECK(unicode_category_approx(0x01) == UnicodeCategory::Control);
}

static void test_is_combining_mark() {
    CHECK(is_combining_mark(0x0300) == true);   // combining grave accent
    CHECK(is_combining_mark(0x036F) == true);   // last in block
    CHECK(is_combining_mark('a') == false);
    CHECK(is_combining_mark(0x0041) == false);  // A
}

static void test_is_whitespace() {
    CHECK(is_unicode_whitespace(' ') == true);
    CHECK(is_unicode_whitespace('\t') == true);
    CHECK(is_unicode_whitespace('\n') == true);
    CHECK(is_unicode_whitespace('a') == false);
    CHECK(is_unicode_whitespace(0x00A0) == true);   // non-breaking space
    CHECK(is_unicode_whitespace(0x3000) == true);   // ideographic space
}

static void test_is_rtl_strong() {
    CHECK(is_rtl_strong(0x05D0) == true);   // Hebrew alef
    CHECK(is_rtl_strong(0x0627) == true);   // Arabic alef
    CHECK(is_rtl_strong('A') == false);
    CHECK(is_rtl_strong('1') == false);
}

static void test_shaper_rtl_detection() {
    std::vector<uint32_t> arabic_text = {0x0627, 0x0644, 0x0639, 0x0631, 0x0628};
    CHECK(text_likely_rtl(arabic_text) == true);

    std::vector<uint32_t> latin_text = {'h', 'e', 'l', 'l', 'o'};
    CHECK(text_likely_rtl(latin_text) == false);
}

static void test_shaper_cluster_spans() {
    std::vector<ShapedGlyph> glyphs = {
        {1, 0, 100, 0, 0, 0},
        {2, 0, 100, 0, 0, 0},  // same cluster as previous (e.g. ligature)
        {3, 1, 100, 0, 0, 0},
        {4, 2, 100, 0, 0, 0},
    };
    auto spans = cluster_spans(glyphs);
    CHECK(spans.size() == 3);
    CHECK(spans[0].first == 0 && spans[0].second == 2);
    CHECK(spans[1].first == 2 && spans[1].second == 3);
    CHECK(spans[2].first == 3 && spans[2].second == 4);
}

static void test_shaper_reverse_glyphs() {
    std::vector<ShapedGlyph> glyphs = {
        {1, 0, 100, 0, 0, 0},
        {2, 1, 100, 0, 0, 0},
        {3, 2, 100, 0, 0, 0},
    };
    reverse_glyphs(glyphs);
    CHECK(glyphs[0].glyph_id == 3);
    CHECK(glyphs[1].glyph_id == 2);
    CHECK(glyphs[2].glyph_id == 1);
}

static void test_gsub_single_subst() {
    std::unordered_map<uint16_t,uint16_t> coverage = {{10, 0}, {20, 1}};
    std::vector<uint16_t> subst = {100, 200};
    CHECK(gsub_single_subst(10, coverage, subst) == 100);
    CHECK(gsub_single_subst(20, coverage, subst) == 200);
    CHECK(gsub_single_subst(99, coverage, subst) == 0);
}

int main() {
    test_utf8_ascii();
    test_utf8_two_byte();
    test_utf8_three_byte();
    test_utf8_four_byte();
    test_utf8_mixed();
    test_utf8_roundtrip_empty();
    test_tag_to_str();
    test_feature_on_by_default();
    test_find_feature_info();
    test_parse_feature_string();
    test_format_feature_string();
    test_unicode_category();
    test_is_combining_mark();
    test_is_whitespace();
    test_is_rtl_strong();
    test_shaper_rtl_detection();
    test_shaper_cluster_spans();
    test_shaper_reverse_glyphs();
    test_gsub_single_subst();

    if (failures) {
        fprintf(stderr, "test_layout: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_layout: all passed\n");
    return 0;
}
