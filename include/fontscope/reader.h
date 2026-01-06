#pragma once
#include <cstdint>
#include <cstddef>
#include "fontscope/errors.h"

namespace fontscope {

// Bounded big-endian binary reader. All multi-byte reads use network byte order
// (big-endian), matching the OpenType specification.
class ByteReader {
public:
    ByteReader() : data_(nullptr), size_(0), pos_(0), error_(false) {}
    ByteReader(const uint8_t* data, size_t size)
        : data_(data), size_(size), pos_(0), error_(false) {}

    bool ok()          const { return !error_; }
    bool eof()         const { return pos_ >= size_; }
    size_t pos()       const { return pos_; }
    size_t size()      const { return size_; }
    size_t remaining() const { return (pos_ < size_) ? (size_ - pos_) : 0; }

    const uint8_t* data() const { return data_; }
    const uint8_t* current() const {
        return (data_ && pos_ < size_) ? (data_ + pos_) : nullptr;
    }

    void seek(size_t offset) {
        if (offset > size_) { error_ = true; return; }
        pos_ = offset;
    }

    void skip(size_t n) {
        if (n > size_ - pos_) { error_ = true; return; }
        pos_ += n;
    }

    uint8_t read_u8() {
        if (pos_ + 1 > size_) { error_ = true; return 0; }
        return data_[pos_++];
    }

    int8_t read_i8() {
        return static_cast<int8_t>(read_u8());
    }

    uint16_t read_u16_be() {
        if (pos_ + 2 > size_) { error_ = true; return 0; }
        uint16_t v = (uint16_t(data_[pos_]) << 8) | data_[pos_+1];
        pos_ += 2;
        return v;
    }

    int16_t read_i16_be() {
        return static_cast<int16_t>(read_u16_be());
    }

    uint32_t read_u24_be() {
        if (pos_ + 3 > size_) { error_ = true; return 0; }
        uint32_t v = (uint32_t(data_[pos_]) << 16)
                   | (uint32_t(data_[pos_+1]) << 8)
                   |  uint32_t(data_[pos_+2]);
        pos_ += 3;
        return v;
    }

    uint32_t read_u32_be() {
        if (pos_ + 4 > size_) { error_ = true; return 0; }
        uint32_t v = (uint32_t(data_[pos_])   << 24)
                   | (uint32_t(data_[pos_+1]) << 16)
                   | (uint32_t(data_[pos_+2]) << 8)
                   |  uint32_t(data_[pos_+3]);
        pos_ += 4;
        return v;
    }

    int32_t read_i32_be() {
        return static_cast<int32_t>(read_u32_be());
    }

    // Returns a sub-reader covering exactly [offset, offset+length) of the
    // parent buffer. Error-state is copied from parent if parent is already bad.
    ByteReader sub_reader(size_t offset, size_t length) const {
        if (!data_ || offset > size_ || length > size_ - offset)
            return ByteReader(data_, 0);
        return ByteReader(data_ + offset, length);
    }

    // Read exactly n bytes into out[]; returns false on truncation.
    bool read_bytes(uint8_t* out, size_t n) {
        if (pos_ + n > size_) { error_ = true; return false; }
        for (size_t i = 0; i < n; ++i)
            out[i] = data_[pos_++];
        return true;
    }

private:
    const uint8_t* data_;
    size_t         size_;
    size_t         pos_;
    bool           error_;
};

} // namespace fontscope
