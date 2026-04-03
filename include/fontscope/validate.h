#pragma once
#include <string>
#include <vector>
#include "fontscope/inspect.h"

namespace fontscope {

enum class IssueSeverity { Info, Warning, Error };

struct ValidationIssue {
    IssueSeverity severity;
    std::string   table;
    std::string   message;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;
    int error_count{0};
    int warning_count{0};

    bool ok() const { return error_count == 0; }

    void add(IssueSeverity sev, const char* table, std::string msg) {
        issues.push_back({sev, table, std::move(msg)});
        if (sev == IssueSeverity::Error)   ++error_count;
        if (sev == IssueSeverity::Warning) ++warning_count;
    }
};

ValidationReport validate_font(const FontFace& font);
void print_validation_report(const ValidationReport& report);

} // namespace fontscope
