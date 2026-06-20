#pragma once
#include <string>
#include <cstdint>
#include "fontscope/inspect.h"
#include "fontscope/validate.h"
#include "fontscope/glyf.h"
#include "fontscope/outline_stats.h"

namespace fontscope {

// Minimal JSON serialization for fontscope output (no third-party dependency).
// Output is compact, newline-delimited key-value JSON.

// Serialize the font info summary to a JSON string.
std::string font_info_to_json(const FontFace& font);

// Serialize the table list to a JSON array.
std::string tables_to_json(const FontFace& font);

// Serialize a validation report to JSON.
std::string validation_report_to_json(const ValidationReport& report);

// Serialize a single glyph outline to JSON.
std::string glyph_to_json(const RawGlyph& g, uint16_t glyph_id);

// Serialize cmap subtables + a Unicode mapping sample to JSON.
std::string cmap_to_json(const FontFace& font, uint32_t cp_start, uint32_t cp_end);

// Serialize name records to JSON.
std::string name_table_to_json(const FontFace& font);

// Serialize variation axis descriptors to JSON.
std::string fvar_to_json(const FontFace& font);

// Serialize an outline summary to JSON.
std::string outline_summary_to_json(const OutlineSummary& s);

// Escape a string for JSON: replaces \, ", control chars.
std::string json_escape(const std::string& s);

// Simple JSON builder helper.
class JsonWriter {
public:
    JsonWriter() = default;
    void begin_object() { comma_if_needed(); buf_ += '{'; sep_ = false; }
    void end_object()   { buf_ += '}'; sep_ = true; }
    void begin_array()  { comma_if_needed(); buf_ += '['; sep_ = false; }
    void end_array()    { buf_ += ']'; sep_ = true; }

    void key(const char* k);
    void value_string(const std::string& v);
    void value_string(const char* v);
    void value_int(int64_t v);
    void value_uint(uint64_t v);
    void value_double(double v, int precision = 4);
    void value_bool(bool v);
    void value_null();

    void kv_string(const char* k, const std::string& v)   { key(k); value_string(v); }
    void kv_string(const char* k, const char* v)          { key(k); value_string(v); }
    void kv_int(const char* k, int64_t v)                  { key(k); value_int(v); }
    void kv_uint(const char* k, uint64_t v)                { key(k); value_uint(v); }
    void kv_bool(const char* k, bool v)                    { key(k); value_bool(v); }
    void kv_double(const char* k, double v, int p = 4)    { key(k); value_double(v, p); }

    const std::string& str() const { return buf_; }

private:
    std::string buf_;
    bool sep_{false};

    void comma_if_needed() {
        if (sep_) buf_ += ',';
        sep_ = true;
    }
};

} // namespace fontscope
