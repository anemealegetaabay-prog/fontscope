#pragma once
#include <string>

namespace fontscope {

enum class Status {
    Ok = 0,
    TruncatedInput,
    InvalidMagic,
    UnsupportedVersion,
    MalformedTable,
    TableNotFound,
    GlyphNotFound,
    BadOffset,
    BadChecksum,
    OutOfRange,
    InvalidGlyphId,
    UnsupportedFormat,
    EmptyInput,
};

template <typename T>
struct Result {
    T     value;
    Status status;

    bool ok() const { return status == Status::Ok; }

    static Result<T> success(T v) { return {std::move(v), Status::Ok}; }
    static Result<T> error(Status s) { return {{}, s}; }
};

inline const char* status_string(Status s) {
    switch (s) {
    case Status::Ok:                return "ok";
    case Status::TruncatedInput:    return "truncated input";
    case Status::InvalidMagic:      return "invalid magic";
    case Status::UnsupportedVersion:return "unsupported version";
    case Status::MalformedTable:    return "malformed table";
    case Status::TableNotFound:     return "table not found";
    case Status::GlyphNotFound:     return "glyph not found";
    case Status::BadOffset:         return "bad offset";
    case Status::BadChecksum:       return "bad checksum";
    case Status::OutOfRange:        return "out of range";
    case Status::InvalidGlyphId:    return "invalid glyph id";
    case Status::UnsupportedFormat: return "unsupported format";
    case Status::EmptyInput:        return "empty input";
    default:                        return "unknown error";
    }
}

} // namespace fontscope
