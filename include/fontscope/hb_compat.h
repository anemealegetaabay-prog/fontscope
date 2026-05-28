#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace fontscope {

// OpenType feature tag utilities and registered-feature descriptions.
// This module documents the canonical features from the OpenType specification.

// A registered OpenType feature.
struct OtFeatureInfo {
    uint32_t    tag;          // 4-byte tag as big-endian uint32
    const char* mnemonic;     // e.g., "liga"
    const char* name;         // human-readable name
    bool        on_by_default;
};

// Return all registered features.
const std::vector<OtFeatureInfo>& all_registered_features();

// Look up a feature by tag. Returns nullptr if not in the registered list.
const OtFeatureInfo* find_feature_info(uint32_t tag);
const OtFeatureInfo* find_feature_info(const char* mnemonic);

// Convert a 4-byte ASCII string to a tag uint32.
// Pads with spaces if < 4 chars; ignores chars beyond 4.
uint32_t str_to_tag(const char* s);
uint32_t str_to_tag(const std::string& s);

// Convert a tag to a 4-char string (no null terminator padding displayed).
std::string tag_to_str(uint32_t tag);

// Named script/language pair.
struct ScriptLangName {
    uint32_t    tag;
    const char* name;
};

// Well-known script names.
const std::vector<ScriptLangName>& known_scripts();

// Well-known OpenType language system tags.
const std::vector<ScriptLangName>& known_languages();

// Determine whether a feature is typically ON or OFF by default
// in typesetting pipelines (e.g. liga=on, dlig=off, smcp=off).
bool feature_on_by_default(uint32_t tag);

// Map a Unicode codepoint to its general Unicode category.
enum class UnicodeCategory : uint8_t {
    UppercaseLetter   = 0,
    LowercaseLetter   = 1,
    TitlecaseLetter   = 2,
    ModifierLetter    = 3,
    OtherLetter       = 4,
    NonspacingMark    = 5,
    SpacingMark       = 6,
    EnclosingMark     = 7,
    DecimalNumber     = 8,
    LetterNumber      = 9,
    OtherNumber       = 10,
    ConnectorPunct    = 11,
    DashPunct         = 12,
    OpenPunct         = 13,
    ClosePunct        = 14,
    InitialPunct      = 15,
    FinalPunct        = 16,
    OtherPunct        = 17,
    MathSymbol        = 18,
    CurrencySymbol    = 19,
    ModifierSymbol    = 20,
    OtherSymbol       = 21,
    SpaceSeparator    = 22,
    LineSeparator     = 23,
    ParagraphSeparator = 24,
    Control           = 25,
    Format            = 26,
    Surrogate         = 27,
    PrivateUse        = 28,
    Unassigned        = 29,
};

// Return an approximate Unicode category for a codepoint (simplified, not full UCD).
UnicodeCategory unicode_category_approx(uint32_t cp);

// Returns true if the codepoint is a Unicode mark (combining character).
bool is_combining_mark(uint32_t cp);

// Returns true if the codepoint is whitespace.
bool is_unicode_whitespace(uint32_t cp);

// Returns true if the codepoint is a bidirectional strong RTL character.
bool is_rtl_strong(uint32_t cp);

// Normalize a feature flag string: parse comma-separated "+feat" or "-feat"
// tokens into a map of tag → enabled.
std::vector<std::pair<uint32_t,bool>> parse_feature_string(const std::string& s);

// Format a feature list as a comma-separated string like "+liga,+kern,-calt".
std::string format_feature_string(const std::vector<std::pair<uint32_t,bool>>& features);

} // namespace fontscope
