#include "fontscope/hb_compat.h"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace fontscope {

// Helper: build a tag from 4 ASCII chars at compile time.
static constexpr uint32_t T(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) <<  8) |  uint32_t(uint8_t(d));
}

static const std::vector<OtFeatureInfo> kRegisteredFeatures = {
    {T('a','a','l','t'), "aalt", "Access All Alternates",              false},
    {T('a','b','v','f'), "abvf", "Above-Base Forms",                   true },
    {T('a','b','v','m'), "abvm", "Above-Base Mark Positioning",        true },
    {T('a','b','v','s'), "abvs", "Above-Base Substitutions",           true },
    {T('a','f','r','c'), "afrc", "Alternative Fractions",              false},
    {T('a','k','h','n'), "akhn", "Akhand",                             true },
    {T('b','l','w','f'), "blwf", "Below-base Forms",                   true },
    {T('b','l','w','m'), "blwm", "Below-base Mark Positioning",        true },
    {T('b','l','w','s'), "blws", "Below-base Substitutions",           true },
    {T('c','a','l','t'), "calt", "Contextual Alternates",              true },
    {T('c','a','s','e'), "case", "Case-Sensitive Forms",               false},
    {T('c','c','m','p'), "ccmp", "Glyph Composition/Decomposition",    true },
    {T('c','f','a','r'), "cfar", "Conjunct Form After Ro",             true },
    {T('c','h','c','f'), "chcf", "Conjunct Half Forms",                true },
    {T('c','l','i','g'), "clig", "Contextual Ligatures",               true },
    {T('c','p','c','t'), "cpct", "Centered CJK Punctuation",           false},
    {T('c','p','s','p'), "cpsp", "Capital Spacing",                    false},
    {T('c','s','w','a'), "cswa", "Contextual Swash",                   false},
    {T('c','v','a','r'), "cvar", "CVT Variations",                     true },
    {T('c','2','s','c'), "c2sc", "Small Capitals From Capitals",       false},
    {T('d','i','s','t'), "dist", "Distances",                          true },
    {T('d','l','i','g'), "dlig", "Discretionary Ligatures",            false},
    {T('d','n','o','m'), "dnom", "Denominators",                       false},
    {T('d','t','l','s'), "dtls", "Dotless Forms",                      false},
    {T('e','x','p','t'), "expt", "Expert Forms",                       false},
    {T('f','a','l','t'), "falt", "Final Glyph on Line Alternates",     false},
    {T('f','i','n','2'), "fin2", "Terminal Forms #2",                  true },
    {T('f','i','n','3'), "fin3", "Terminal Forms #3",                  true },
    {T('f','l','a','c'), "flac", "Flattened Accent Forms",             true },
    {T('f','n','a','l'), "fnal", "Final Forms",                        true },
    {T('f','r','a','c'), "frac", "Fractions",                          false},
    {T('f','w','i','d'), "fwid", "Full Widths",                        false},
    {T('h','a','l','f'), "half", "Half Forms",                         true },
    {T('h','a','l','n'), "haln", "Halant Forms",                       true },
    {T('h','a','l','t'), "halt", "Alternate Half Widths",              false},
    {T('h','i','s','t'), "hist", "Historical Forms",                   false},
    {T('h','n','g','l'), "hngl", "Hangul",                             false},
    {T('h','o','j','o'), "hojo", "Hojo Kanji Forms",                   false},
    {T('h','w','i','d'), "hwid", "Half Widths",                        false},
    {T('i','n','i','t'), "init", "Initial Forms",                      true },
    {T('i','s','o','l'), "isol", "Isolated Forms",                     true },
    {T('i','t','a','l'), "ital", "Italics",                            false},
    {T('j','a','l','t'), "jalt", "Justification Alternates",           false},
    {T('j','p','0','4'), "jp04", "JIS2004 Forms",                      false},
    {T('j','p','7','8'), "jp78", "JIS78 Forms",                        false},
    {T('j','p','8','3'), "jp83", "JIS83 Forms",                        false},
    {T('j','p','9','0'), "jp90", "JIS90 Forms",                        false},
    {T('k','e','r','n'), "kern", "Kerning",                            true },
    {T('l','f','b','d'), "lfbd", "Left Bounds",                        false},
    {T('l','i','g','a'), "liga", "Standard Ligatures",                 true },
    {T('l','j','m','o'), "ljmo", "Leading Jamo Forms",                 true },
    {T('l','n','u','m'), "lnum", "Lining Figures",                     false},
    {T('l','o','c','l'), "locl", "Localized Forms",                    true },
    {T('l','t','r','m'), "ltrm", "Left-to-Right Mirrored Forms",       true },
    {T('m','a','r','k'), "mark", "Mark Positioning",                   true },
    {T('m','e','d','i'), "medi", "Medial Forms",                       true },
    {T('m','k','m','k'), "mkmk", "Mark-to-Mark Positioning",           true },
    {T('m','s','e','t'), "mset", "Mark Positioning via Substitution",  true },
    {T('n','a','l','t'), "nalt", "Alternate Annotation Forms",         false},
    {T('n','l','c','k'), "nlck", "NLC Kanji Forms",                    false},
    {T('n','u','m','r'), "numr", "Numerators",                         false},
    {T('o','n','u','m'), "onum", "Oldstyle Figures",                   false},
    {T('o','p','b','d'), "opbd", "Optical Bounds",                     false},
    {T('o','r','d','n'), "ordn", "Ordinals",                           false},
    {T('o','r','n','m'), "ornm", "Ornaments",                          false},
    {T('p','n','u','m'), "pnum", "Proportional Figures",               false},
    {T('p','r','e','p'), "prep", "Pre-base Substitutions",             true },
    {T('p','s','t','f'), "pstf", "Post-base Forms",                    true },
    {T('p','s','t','s'), "psts", "Post-base Substitutions",            true },
    {T('p','w','i','d'), "pwid", "Proportional Widths",                false},
    {T('q','w','i','d'), "qwid", "Quarter Widths",                     false},
    {T('r','a','n','d'), "rand", "Randomize",                          false},
    {T('r','c','l','t'), "rclt", "Required Contextual Alternates",     true },
    {T('r','l','i','g'), "rlig", "Required Ligatures",                 true },
    {T('r','t','l','m'), "rtlm", "Right-to-Left Mirrored Forms",       true },
    {T('r','u','b','y'), "ruby", "Ruby Notation Forms",                false},
    {T('r','v','r','n'), "rvrn", "Required Variation Alternates",      true },
    {T('s','a','l','t'), "salt", "Stylistic Alternates",               false},
    {T('s','i','n','f'), "sinf", "Scientific Inferiors",               false},
    {T('s','m','c','p'), "smcp", "Small Capitals",                     false},
    {T('s','m','p','l'), "smpl", "Simplified Forms",                   false},
    {T('s','s','0','1'), "ss01", "Stylistic Set 1",                    false},
    {T('s','s','0','2'), "ss02", "Stylistic Set 2",                    false},
    {T('s','s','0','3'), "ss03", "Stylistic Set 3",                    false},
    {T('s','s','0','4'), "ss04", "Stylistic Set 4",                    false},
    {T('s','s','0','5'), "ss05", "Stylistic Set 5",                    false},
    {T('s','t','c','h'), "stch", "Stretching Glyph Decomposition",     true },
    {T('s','u','b','s'), "subs", "Subscript",                          false},
    {T('s','u','p','s'), "sups", "Superscript",                        false},
    {T('s','w','s','h'), "swsh", "Swash",                              false},
    {T('t','i','t','l'), "titl", "Titling",                            false},
    {T('t','j','m','o'), "tjmo", "Trailing Jamo Forms",                true },
    {T('t','n','a','m'), "tnam", "Traditional Name Forms",             false},
    {T('t','n','u','m'), "tnum", "Tabular Figures",                    false},
    {T('t','r','a','d'), "trad", "Traditional Forms",                  false},
    {T('t','w','i','d'), "twid", "Third Widths",                       false},
    {T('u','n','i','c'), "unic", "Unicase",                            false},
    {T('v','a','l','t'), "valt", "Alternate Vertical Metrics",         false},
    {T('v','a','t','u'), "vatu", "Vattu Variants",                     true },
    {T('v','e','r','t'), "vert", "Vertical Alternates",                false},
    {T('v','h','a','l'), "vhal", "Alternate Vertical Half Metrics",    false},
    {T('v','j','m','o'), "vjmo", "Vowel Jamo Forms",                   true },
    {T('v','k','n','a'), "vkna", "Vertical Kana Alternates",           false},
    {T('v','k','r','n'), "vkrn", "Vertical Kerning",                   false},
    {T('v','r','t','2'), "vrt2", "Vertical Alternates and Rotation",   false},
    {T('z','e','r','o'), "zero", "Slashed Zero",                       false},
};

const std::vector<OtFeatureInfo>& all_registered_features() {
    return kRegisteredFeatures;
}

const OtFeatureInfo* find_feature_info(uint32_t tag) {
    for (const auto& f : kRegisteredFeatures)
        if (f.tag == tag) return &f;
    return nullptr;
}

const OtFeatureInfo* find_feature_info(const char* mnemonic) {
    for (const auto& f : kRegisteredFeatures)
        if (std::strcmp(f.mnemonic, mnemonic) == 0) return &f;
    return nullptr;
}

uint32_t str_to_tag(const char* s) {
    uint32_t t = 0;
    for (int i = 0; i < 4; ++i) {
        t <<= 8;
        if (s && s[i]) t |= uint8_t(s[i]);
        else            t |= uint8_t(' ');
    }
    return t;
}

uint32_t str_to_tag(const std::string& s) {
    char buf[5] = "    ";
    for (int i = 0; i < 4 && i < int(s.size()); ++i) buf[i] = s[i];
    return str_to_tag(buf);
}

std::string tag_to_str(uint32_t tag) {
    std::string s(4, ' ');
    s[0] = char((tag >> 24) & 0xFF);
    s[1] = char((tag >> 16) & 0xFF);
    s[2] = char((tag >>  8) & 0xFF);
    s[3] = char((tag      ) & 0xFF);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

static const std::vector<ScriptLangName> kKnownScripts = {
    {T('D','F','L','T'), "DFLT — Default"},
    {T('l','a','t','n'), "latn — Latin"},
    {T('c','y','r','l'), "cyrl — Cyrillic"},
    {T('g','r','e','k'), "grek — Greek"},
    {T('a','r','a','b'), "arab — Arabic"},
    {T('h','e','b','r'), "hebr — Hebrew"},
    {T('t','h','a','i'), "thai — Thai"},
    {T('k','a','n','a'), "kana — Katakana"},
    {T('h','a','n','i'), "hani — CJK Unified Ideographs"},
    {T('d','e','v','a'), "deva — Devanagari"},
    {T('b','e','n','g'), "beng — Bengali"},
    {T('t','e','l','u'), "telu — Telugu"},
    {T('t','a','m','l'), "taml — Tamil"},
    {T('g','u','j','r'), "gujr — Gujarati"},
    {T('g','u','r','u'), "guru — Gurmukhi"},
    {T('k','n','d','a'), "knda — Kannada"},
    {T('m','l','y','m'), "mlym — Malayalam"},
    {T('o','r','y','a'), "orya — Oriya"},
    {T('s','i','n','h'), "sinh — Sinhala"},
    {T('m','y','m','r'), "mymr — Myanmar"},
    {T('k','h','m','r'), "khmr — Khmer"},
    {T('l','a','o','o'), "laoo — Lao"},
    {T('t','i','b','t'), "tibt — Tibetan"},
    {T('e','t','h','i'), "ethi — Ethiopic"},
    {T('g','e','o','r'), "geor — Georgian"},
    {T('a','r','m','n'), "armn — Armenian"},
};

const std::vector<ScriptLangName>& known_scripts() { return kKnownScripts; }

static const std::vector<ScriptLangName> kKnownLanguages = {
    {T('A','F','K',' '), "AFK — Afrikaans"},
    {T('A','M','H',' '), "AMH — Amharic"},
    {T('A','R','A',' '), "ARA — Arabic"},
    {T('A','S','M',' '), "ASM — Assamese"},
    {T('B','E','L',' '), "BEL — Byelorussian"},
    {T('B','E','N',' '), "BEN — Bengali"},
    {T('B','G','R',' '), "BGR — Bulgarian"},
    {T('B','R','M',' '), "BRM — Burmese"},
    {T('C','A','T',' '), "CAT — Catalan"},
    {T('C','H','N',' '), "CHN — Chinese (Simplified)"},
    {T('D','E','U',' '), "DEU — German"},
    {T('E','N','G',' '), "ENG — English"},
    {T('E','S','P',' '), "ESP — Spanish"},
    {T('E','T','H',' '), "ETH — Ethiopic"},
    {T('F','A','R',' '), "FAR — Farsi (Persian)"},
    {T('F','I','N',' '), "FIN — Finnish"},
    {T('F','R','A',' '), "FRA — French"},
    {T('G','E','O',' '), "GEO — Georgian"},
    {T('G','E','R',' '), "GER — German (Standard)"},
    {T('G','U','J',' '), "GUJ — Gujarati"},
    {T('H','E','B',' '), "HEB — Hebrew"},
    {T('H','I','N',' '), "HIN — Hindi"},
    {T('H','R','I',' '), "HRI — Croatian"},
    {T('H','U','R',' '), "HUR — Hungarian"},
    {T('I','N','D',' '), "IND — Indonesian"},
    {T('I','T','A',' '), "ITA — Italian"},
    {T('J','A','P',' '), "JAP — Japanese"},
    {T('K','A','N',' '), "KAN — Kannada"},
    {T('K','H','M',' '), "KHM — Khmer"},
    {T('K','O','R',' '), "KOR — Korean"},
    {T('L','A','O',' '), "LAO — Lao"},
    {T('L','A','T',' '), "LAT — Latin"},
    {T('L','V','I',' '), "LVI — Latvian"},
    {T('L','T','H',' '), "LTH — Lithuanian"},
    {T('M','A','L',' '), "MAL — Malayalam"},
    {T('M','A','R',' '), "MAR — Marathi"},
    {T('M','N','G',' '), "MNG — Mongolian"},
    {T('N','E','P',' '), "NEP — Nepali"},
    {T('N','L','D',' '), "NLD — Dutch"},
    {T('N','O','R',' '), "NOR — Norwegian"},
    {T('O','R','I',' '), "ORI — Oriya"},
    {T('P','A','N',' '), "PAN — Punjabi"},
    {T('P','L','K',' '), "PLK — Polish"},
    {T('P','O','R',' '), "POR — Portuguese"},
    {T('R','U','S',' '), "RUS — Russian"},
    {T('S','A','N',' '), "SAN — Sanskrit"},
    {T('S','N','D',' '), "SND — Sindhi"},
    {T('S','R','B',' '), "SRB — Serbian"},
    {T('S','W','E',' '), "SWE — Swedish"},
    {T('S','Y','R',' '), "SYR — Syriac"},
    {T('T','A','M',' '), "TAM — Tamil"},
    {T('T','E','L',' '), "TEL — Telugu"},
    {T('T','H','I',' '), "THI — Thai"},
    {T('T','I','B',' '), "TIB — Tibetan"},
    {T('T','K','M',' '), "TKM — Turkmen"},
    {T('T','R','K',' '), "TRK — Turkish"},
    {T('U','K','R',' '), "UKR — Ukrainian"},
    {T('U','R','D',' '), "URD — Urdu"},
    {T('U','Z','B',' '), "UZB — Uzbek"},
    {T('V','I','T',' '), "VIT — Vietnamese"},
    {T('W','E','L',' '), "WEL — Welsh"},
    {T('Y','I','D',' '), "YID — Yiddish"},
    {T('Z','H','S',' '), "ZHS — Chinese (Simplified)"},
    {T('Z','H','T',' '), "ZHT — Chinese (Traditional)"},
};

const std::vector<ScriptLangName>& known_languages() { return kKnownLanguages; }

bool feature_on_by_default(uint32_t tag) {
    static const uint32_t on_tags[] = {
        T('c','a','l','t'),
        T('c','c','m','p'),
        T('c','l','i','g'),
        T('k','e','r','n'),
        T('l','i','g','a'),
        T('l','o','c','l'),
        T('m','a','r','k'),
        T('m','k','m','k'),
        T('r','l','i','g'),
        T('r','v','r','n'),
    };
    for (uint32_t t : on_tags)
        if (t == tag) return true;
    return false;
}

UnicodeCategory unicode_category_approx(uint32_t cp) {
    if (cp < 0x20)  return UnicodeCategory::Control;
    if (cp == 0x20) return UnicodeCategory::SpaceSeparator;
    if (cp >= 0x21 && cp <= 0x2F) return UnicodeCategory::OtherPunct;
    if (cp >= 0x30 && cp <= 0x39) return UnicodeCategory::DecimalNumber;
    if (cp >= 0x3A && cp <= 0x40) return UnicodeCategory::OtherPunct;
    if (cp >= 0x41 && cp <= 0x5A) return UnicodeCategory::UppercaseLetter;
    if (cp >= 0x5B && cp <= 0x60) return UnicodeCategory::OtherPunct;
    if (cp >= 0x61 && cp <= 0x7A) return UnicodeCategory::LowercaseLetter;
    if (cp >= 0x7B && cp <= 0x7E) return UnicodeCategory::OtherPunct;
    if (cp == 0x7F) return UnicodeCategory::Control;
    if (cp >= 0x0300 && cp <= 0x036F) return UnicodeCategory::NonspacingMark;
    if (cp >= 0x0600 && cp <= 0x06FF) return UnicodeCategory::OtherLetter;
    if (cp >= 0x0900 && cp <= 0x097F) return UnicodeCategory::OtherLetter;
    if (cp >= 0x4E00 && cp <= 0x9FFF) return UnicodeCategory::OtherLetter;
    if (cp >= 0xAC00 && cp <= 0xD7A3) return UnicodeCategory::OtherLetter;
    return UnicodeCategory::OtherLetter;
}

bool is_combining_mark(uint32_t cp) {
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return true;
    if (cp >= 0x20D0 && cp <= 0x20FF) return true;
    if (cp >= 0xFE20 && cp <= 0xFE2F) return true;
    return false;
}

bool is_unicode_whitespace(uint32_t cp) {
    switch (cp) {
    case 0x0009: case 0x000A: case 0x000B: case 0x000C:
    case 0x000D: case 0x0020: case 0x0085: case 0x00A0:
    case 0x1680: case 0x2000: case 0x2001: case 0x2002:
    case 0x2003: case 0x2004: case 0x2005: case 0x2006:
    case 0x2007: case 0x2008: case 0x2009: case 0x200A:
    case 0x2028: case 0x2029: case 0x202F: case 0x205F:
    case 0x3000:
        return true;
    default:
        return false;
    }
}

bool is_rtl_strong(uint32_t cp) {
    if (cp >= 0x0590 && cp <= 0x05FF) return true;
    if (cp >= 0x0600 && cp <= 0x06FF) return true;
    if (cp >= 0x0700 && cp <= 0x074F) return true;
    if (cp >= 0x0750 && cp <= 0x077F) return true;
    if (cp >= 0x0800 && cp <= 0x083F) return true;
    if (cp >= 0xFB00 && cp <= 0xFB4F) return true;
    if (cp >= 0xFB50 && cp <= 0xFDFF) return true;
    if (cp >= 0xFE70 && cp <= 0xFEFF) return true;
    return false;
}

std::vector<std::pair<uint32_t,bool>> parse_feature_string(const std::string& s) {
    std::vector<std::pair<uint32_t,bool>> result;
    std::string token;
    std::istringstream ss(s);
    while (std::getline(ss, token, ',')) {
        if (token.empty()) continue;
        bool enable = true;
        size_t start = 0;
        if (!token.empty() && (token[0] == '+' || token[0] == '-')) {
            enable = (token[0] == '+');
            start = 1;
        }
        std::string tag_str = token.substr(start);
        if (!tag_str.empty())
            result.emplace_back(str_to_tag(tag_str.c_str()), enable);
    }
    return result;
}

std::string format_feature_string(const std::vector<std::pair<uint32_t,bool>>& features) {
    std::string out;
    for (const auto& [tag, en] : features) {
        if (!out.empty()) out += ',';
        out += en ? '+' : '-';
        out += tag_to_str(tag);
    }
    return out;
}

} // namespace fontscope
