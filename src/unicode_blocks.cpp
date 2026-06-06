#include "fontscope/unicode_blocks.h"
#include <algorithm>

namespace fontscope {

// A representative subset of Unicode 15.0 blocks.
// Source: https://www.unicode.org/Public/15.0.0/ucd/Blocks.txt
static const std::vector<UnicodeBlock> kBlocks = {
    {0x0000, 0x007F, "Basic Latin",                        "ASCII"     },
    {0x0080, 0x00FF, "Latin-1 Supplement",                 "Lat1Sup"   },
    {0x0100, 0x017F, "Latin Extended-A",                   "LatExtA"   },
    {0x0180, 0x024F, "Latin Extended-B",                   "LatExtB"   },
    {0x0250, 0x02AF, "IPA Extensions",                     "IPA"       },
    {0x02B0, 0x02FF, "Spacing Modifier Letters",           "SpacMod"   },
    {0x0300, 0x036F, "Combining Diacritical Marks",        "CombDiac"  },
    {0x0370, 0x03FF, "Greek and Coptic",                   "Greek"     },
    {0x0400, 0x04FF, "Cyrillic",                           "Cyrl"      },
    {0x0500, 0x052F, "Cyrillic Supplement",                "CyrlSup"   },
    {0x0530, 0x058F, "Armenian",                           "Armn"      },
    {0x0590, 0x05FF, "Hebrew",                             "Hebr"      },
    {0x0600, 0x06FF, "Arabic",                             "Arab"      },
    {0x0700, 0x074F, "Syriac",                             "Syrc"      },
    {0x0750, 0x077F, "Arabic Supplement",                  "ArabSup"   },
    {0x0900, 0x097F, "Devanagari",                         "Deva"      },
    {0x0980, 0x09FF, "Bengali",                            "Beng"      },
    {0x0A00, 0x0A7F, "Gurmukhi",                           "Guru"      },
    {0x0A80, 0x0AFF, "Gujarati",                           "Gujr"      },
    {0x0B00, 0x0B7F, "Oriya",                              "Orya"      },
    {0x0B80, 0x0BFF, "Tamil",                              "Taml"      },
    {0x0C00, 0x0C7F, "Telugu",                             "Telu"      },
    {0x0C80, 0x0CFF, "Kannada",                            "Knda"      },
    {0x0D00, 0x0D7F, "Malayalam",                          "Mlym"      },
    {0x0D80, 0x0DFF, "Sinhala",                            "Sinh"      },
    {0x0E00, 0x0E7F, "Thai",                               "Thai"      },
    {0x0E80, 0x0EFF, "Lao",                                "Laoo"      },
    {0x0F00, 0x0FFF, "Tibetan",                            "Tibt"      },
    {0x1000, 0x109F, "Myanmar",                            "Mymr"      },
    {0x10A0, 0x10FF, "Georgian",                           "Geor"      },
    {0x1100, 0x11FF, "Hangul Jamo",                        "HangJamo"  },
    {0x1200, 0x137F, "Ethiopic",                           "Ethi"      },
    {0x13A0, 0x13FF, "Cherokee",                           "Cher"      },
    {0x1400, 0x167F, "Unified Canadian Aboriginal Syllabics", "UCAS"   },
    {0x1680, 0x169F, "Ogham",                              "Ogam"      },
    {0x16A0, 0x16FF, "Runic",                              "Runr"      },
    {0x1700, 0x171F, "Tagalog",                            "Tglg"      },
    {0x1750, 0x177F, "Hanunoo",                            "Hano"      },
    {0x1780, 0x17FF, "Khmer",                              "Khmr"      },
    {0x1800, 0x18AF, "Mongolian",                          "Mong"      },
    {0x1C00, 0x1C4F, "Lepcha",                             "Lepc"      },
    {0x1C50, 0x1C7F, "Ol Chiki",                           "Olck"      },
    {0x1D00, 0x1D7F, "Phonetic Extensions",                "PhonExt"   },
    {0x1D80, 0x1DBF, "Phonetic Extensions Supplement",     "PhonExtSup"},
    {0x1DC0, 0x1DFF, "Combining Diacritical Marks Supplement", "CDMSup"},
    {0x1E00, 0x1EFF, "Latin Extended Additional",          "LatExtAdd" },
    {0x1F00, 0x1FFF, "Greek Extended",                     "GrExt"     },
    {0x2000, 0x206F, "General Punctuation",                "GenPunct"  },
    {0x2070, 0x209F, "Superscripts and Subscripts",        "SupSub"    },
    {0x20A0, 0x20CF, "Currency Symbols",                   "CurrSym"   },
    {0x20D0, 0x20FF, "Combining Diacritical Marks for Symbols", "CDMSym"},
    {0x2100, 0x214F, "Letterlike Symbols",                 "LetterSym" },
    {0x2150, 0x218F, "Number Forms",                       "NumForms"  },
    {0x2190, 0x21FF, "Arrows",                             "Arrows"    },
    {0x2200, 0x22FF, "Mathematical Operators",             "MathOp"    },
    {0x2300, 0x23FF, "Miscellaneous Technical",            "MiscTech"  },
    {0x2400, 0x243F, "Control Pictures",                   "CtrlPic"   },
    {0x2440, 0x245F, "Optical Character Recognition",      "OCR"       },
    {0x2460, 0x24FF, "Enclosed Alphanumerics",             "EncAlpha"  },
    {0x2500, 0x257F, "Box Drawing",                        "BoxDraw"   },
    {0x2580, 0x259F, "Block Elements",                     "BlockElem" },
    {0x25A0, 0x25FF, "Geometric Shapes",                   "GeoShapes" },
    {0x2600, 0x26FF, "Miscellaneous Symbols",              "MiscSym"   },
    {0x2700, 0x27BF, "Dingbats",                           "Dingbats"  },
    {0x27C0, 0x27EF, "Miscellaneous Mathematical Symbols-A","MathSymA" },
    {0x27F0, 0x27FF, "Supplemental Arrows-A",              "SupArrowA" },
    {0x2800, 0x28FF, "Braille Patterns",                   "Braille"   },
    {0x2900, 0x297F, "Supplemental Arrows-B",              "SupArrowB" },
    {0x2980, 0x29FF, "Miscellaneous Mathematical Symbols-B","MathSymB" },
    {0x2A00, 0x2AFF, "Supplemental Mathematical Operators","SupMathOp" },
    {0x2B00, 0x2BFF, "Miscellaneous Symbols and Arrows",   "MiscSymArr"},
    {0x2C00, 0x2C5F, "Glagolitic",                         "Glag"      },
    {0x2C60, 0x2C7F, "Latin Extended-C",                   "LatExtC"   },
    {0x2C80, 0x2CFF, "Coptic",                             "Copt"      },
    {0x2D00, 0x2D2F, "Georgian Supplement",                "GeorSup"   },
    {0x2D30, 0x2D7F, "Tifinagh",                           "Tfng"      },
    {0x2D80, 0x2DDF, "Ethiopic Extended",                  "EthiExt"   },
    {0x2E00, 0x2E7F, "Supplemental Punctuation",           "SupPunct"  },
    {0x2E80, 0x2EFF, "CJK Radicals Supplement",            "CJKRad"    },
    {0x2F00, 0x2FDF, "Kangxi Radicals",                    "Kangxi"    },
    {0x2FF0, 0x2FFF, "Ideographic Description Characters", "IDC"       },
    {0x3000, 0x303F, "CJK Symbols and Punctuation",        "CJKSymPunct"},
    {0x3040, 0x309F, "Hiragana",                           "Hira"      },
    {0x30A0, 0x30FF, "Katakana",                           "Kana"      },
    {0x3100, 0x312F, "Bopomofo",                           "Bopo"      },
    {0x3130, 0x318F, "Hangul Compatibility Jamo",          "HangCompatJamo"},
    {0x3190, 0x319F, "Kanbun",                             "Kanbun"    },
    {0x31A0, 0x31BF, "Bopomofo Extended",                  "BopoExt"   },
    {0x31C0, 0x31EF, "CJK Strokes",                        "CJKStroke" },
    {0x31F0, 0x31FF, "Katakana Phonetic Extensions",       "KanaPhonExt"},
    {0x3200, 0x32FF, "Enclosed CJK Letters and Months",    "EncCJK"    },
    {0x3300, 0x33FF, "CJK Compatibility",                  "CJKCompat" },
    {0x3400, 0x4DBF, "CJK Unified Ideographs Extension A", "CJKA"      },
    {0x4E00, 0x9FFF, "CJK Unified Ideographs",             "CJK"       },
    {0xA000, 0xA48F, "Yi Syllables",                       "Yi"        },
    {0xA490, 0xA4CF, "Yi Radicals",                        "YiRad"     },
    {0xA4D0, 0xA4FF, "Lisu",                               "Lisu"      },
    {0xA500, 0xA63F, "Vai",                                "Vai"       },
    {0xA640, 0xA69F, "Cyrillic Extended-B",                "CyrlExtB"  },
    {0xA700, 0xA71F, "Modifier Tone Letters",              "ModTone"   },
    {0xA720, 0xA7FF, "Latin Extended-D",                   "LatExtD"   },
    {0xA800, 0xA82F, "Syloti Nagri",                       "Sylo"      },
    {0xA840, 0xA87F, "Phags-pa",                           "Phag"      },
    {0xA880, 0xA8DF, "Saurashtra",                         "Saur"      },
    {0xA900, 0xA92F, "Kayah Li",                           "Kali"      },
    {0xA930, 0xA95F, "Rejang",                             "Rjng"      },
    {0xA960, 0xA97F, "Hangul Jamo Extended-A",             "HangJamoExtA"},
    {0xA980, 0xA9DF, "Javanese",                           "Java"      },
    {0xAA00, 0xAA5F, "Cham",                               "Cham"      },
    {0xAA60, 0xAA7F, "Myanmar Extended-A",                 "MymrExtA"  },
    {0xAA80, 0xAADF, "Tai Viet",                           "Tavt"      },
    {0xAB00, 0xAB2F, "Ethiopic Extended-A",                "EthiExtA"  },
    {0xAB30, 0xAB6F, "Latin Extended-E",                   "LatExtE"   },
    {0xAB70, 0xABBF, "Cherokee Supplement",                "CherSup"   },
    {0xABC0, 0xABFF, "Meetei Mayek",                       "Mtei"      },
    {0xAC00, 0xD7AF, "Hangul Syllables",                   "HangSyl"   },
    {0xD7B0, 0xD7FF, "Hangul Jamo Extended-B",             "HangJamoExtB"},
    {0xE000, 0xF8FF, "Private Use Area",                   "PUA"       },
    {0xF900, 0xFAFF, "CJK Compatibility Ideographs",       "CJKCompatIdeo"},
    {0xFB00, 0xFB4F, "Alphabetic Presentation Forms",      "AlphaPF"   },
    {0xFB50, 0xFDFF, "Arabic Presentation Forms-A",        "ArabPFA"   },
    {0xFE00, 0xFE0F, "Variation Selectors",                "VarSel"    },
    {0xFE20, 0xFE2F, "Combining Half Marks",               "CombHalfMk"},
    {0xFE30, 0xFE4F, "CJK Compatibility Forms",            "CJKCompatForms"},
    {0xFE50, 0xFE6F, "Small Form Variants",                "SmallForms"},
    {0xFE70, 0xFEFF, "Arabic Presentation Forms-B",        "ArabPFB"   },
    {0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms",       "HalfFull"  },
    {0xFFF0, 0xFFFF, "Specials",                           "Specials"  },
    {0x10000,0x1007F,"Linear B Syllabary",                  "LinBSyl"  },
    {0x10080,0x100FF,"Linear B Ideograms",                  "LinBIdeo" },
    {0x10100,0x1013F,"Aegean Numbers",                      "AegeNum"  },
    {0x10140,0x1018F,"Ancient Greek Numbers",               "AnGrNum"  },
    {0x10190,0x101CF,"Ancient Symbols",                     "AnSym"    },
    {0x10300,0x1032F,"Old Italic",                          "Ital"     },
    {0x10330,0x1034F,"Gothic",                              "Goth"     },
    {0x10380,0x1039F,"Ugaritic",                            "Ugar"     },
    {0x10400,0x1044F,"Deseret",                             "Dsrt"     },
    {0x10450,0x1047F,"Shavian",                             "Shaw"     },
    {0x10480,0x104AF,"Osmanya",                             "Osma"     },
    {0x10800,0x1083F,"Cypriot Syllabary",                   "Cprt"     },
    {0x1D000,0x1D0FF,"Byzantine Musical Symbols",           "ByzMus"   },
    {0x1D100,0x1D1FF,"Musical Symbols",                     "MusSym"   },
    {0x1D300,0x1D35F,"Tai Xuan Jing Symbols",               "TaiXuan"  },
    {0x1D400,0x1D7FF,"Mathematical Alphanumeric Symbols",   "MathAlpha"},
    {0x1F000,0x1F02F,"Mahjong Tiles",                       "Mahjong"  },
    {0x1F030,0x1F09F,"Domino Tiles",                        "Domino"   },
    {0x1F0A0,0x1F0FF,"Playing Cards",                       "PlayCards"},
    {0x1F300,0x1F5FF,"Miscellaneous Symbols and Pictographs","MiscPicto"},
    {0x1F600,0x1F64F,"Emoticons",                           "Emoticons"},
    {0x1F680,0x1F6FF,"Transport and Map Symbols",           "Transport"},
    {0x1F700,0x1F77F,"Alchemical Symbols",                  "Alchemic" },
    {0x20000,0x2A6DF,"CJK Unified Ideographs Extension B",  "CJKB"    },
    {0x2A700,0x2B73F,"CJK Unified Ideographs Extension C",  "CJKC"    },
    {0x2B740,0x2B81F,"CJK Unified Ideographs Extension D",  "CJKD"    },
    {0x2B820,0x2CEAF,"CJK Unified Ideographs Extension E",  "CJKE"    },
    {0x2CEB0,0x2EBEF,"CJK Unified Ideographs Extension F",  "CJKF"    },
    {0xE0000,0xE007F,"Tags",                                "Tags"    },
    {0xE0100,0xE01EF,"Variation Selectors Supplement",       "VarSelSup"},
    {0xF0000,0xFFFFF,"Supplementary Private Use Area-A",     "SPUA-A"  },
    {0x100000,0x10FFFF,"Supplementary Private Use Area-B",   "SPUA-B"  },
};

const std::vector<UnicodeBlock>& all_unicode_blocks() {
    return kBlocks;
}

std::optional<const UnicodeBlock*> block_for_codepoint(uint32_t cp) {
    // Binary search for the block containing cp.
    int lo = 0, hi = int(kBlocks.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (cp < kBlocks[mid].first) {
            hi = mid - 1;
        } else if (cp > kBlocks[mid].last) {
            lo = mid + 1;
        } else {
            return &kBlocks[mid];
        }
    }
    return std::nullopt;
}

const char* block_name(uint32_t cp) {
    auto r = block_for_codepoint(cp);
    return r ? (*r)->name : "Unknown";
}

std::vector<uint32_t> filter_by_block(
    const std::vector<uint32_t>& codepoints,
    const UnicodeBlock& block)
{
    std::vector<uint32_t> result;
    for (uint32_t cp : codepoints)
        if (cp >= block.first && cp <= block.last) result.push_back(cp);
    return result;
}

std::vector<BlockUsage> count_by_block(const std::vector<uint32_t>& codepoints) {
    std::vector<BlockUsage> usage;
    for (uint32_t cp : codepoints) {
        auto r = block_for_codepoint(cp);
        if (!r) continue;
        size_t idx = size_t(*r - kBlocks.data());
        bool found = false;
        for (auto& u : usage) {
            if (u.block_index == idx) { ++u.count; found = true; break; }
        }
        if (!found) usage.push_back({idx, 1, *r});
    }
    std::sort(usage.begin(), usage.end(),
              [](const BlockUsage& a, const BlockUsage& b){
                  return a.count > b.count;
              });
    return usage;
}

} // namespace fontscope
