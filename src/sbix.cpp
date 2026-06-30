#include "fontscope/sbix.h"
#include <cstring>

namespace fontscope {

Result<SbixTable> parse_sbix(ByteReader& r, uint16_t num_glyphs) {
    SbixTable sbix;
    sbix.version = r.read_u16_be();
    sbix.flags   = r.read_u16_be();
    uint32_t num_strikes = r.read_u32_be();
    if (!r.ok()) return Result<SbixTable>::error(Status::TruncatedInput);

    // Sanity-cap the strike count so a forged length cannot force a huge
    // allocation before we have read any strike data.
    if (num_strikes > 0xFFFF)
        return Result<SbixTable>::error(Status::MalformedTable);

    std::vector<uint32_t> strike_offsets;
    strike_offsets.reserve(num_strikes);
    for (uint32_t i = 0; i < num_strikes; ++i) {
        uint32_t off = r.read_u32_be();
        if (!r.ok()) return Result<SbixTable>::error(Status::TruncatedInput);
        strike_offsets.push_back(off);
    }

    const uint32_t per_glyph = uint32_t(num_glyphs) + 1;  // offsets array length

    for (uint32_t s = 0; s < num_strikes; ++s) {
        ByteReader sr = r.sub_reader(strike_offsets[s],
                                     (strike_offsets[s] <= r.size())
                                         ? (r.size() - strike_offsets[s]) : 0);
        SbixStrike strike;
        strike.ppem = sr.read_u16_be();
        strike.ppi  = sr.read_u16_be();
        if (!sr.ok()) continue;

        // glyphDataOffsets[num_glyphs + 1], each relative to the strike start.
        std::vector<uint32_t> goff;
        goff.reserve(per_glyph);
        for (uint32_t g = 0; g < per_glyph; ++g) {
            uint32_t o = sr.read_u32_be();
            if (!sr.ok()) break;
            goff.push_back(o);
        }
        if (goff.size() < per_glyph) { sbix.strikes.push_back(std::move(strike)); continue; }

        strike.glyphs.resize(num_glyphs);
        for (uint16_t g = 0; g < num_glyphs; ++g) {
            uint32_t begin = goff[g];
            uint32_t end   = goff[g + 1];
            if (end <= begin) continue;          // no bitmap for this glyph
            uint32_t len = end - begin;
            if (len < 8) continue;               // need origin + graphicType

            ByteReader gr = sr.sub_reader(begin, len);
            SbixGlyphData gd;
            gd.origin_offset_x  = gr.read_i16_be();
            gd.origin_offset_y  = gr.read_i16_be();
            gd.graphic_type.value = gr.read_u32_be();
            if (!gr.ok()) continue;

            uint32_t img_len = len - 8;
            gd.data.resize(img_len);
            gr.read_bytes(gd.data.data(), img_len);
            strike.glyphs[g] = std::move(gd);
        }

        sbix.strikes.push_back(std::move(strike));
    }

    return Result<SbixTable>::success(std::move(sbix));
}

SbixImageSize sbix_image_size(const SbixGlyphData& g) {
    SbixImageSize size{0, 0};

    // The embedded image starts with a 4-byte big-endian length field giving
    // the number of payload bytes that follow the field itself.
    if (g.graphic_type != Tag::from_chars("png ")) return size;
    if (g.data.size() < 4) return size;

    const uint8_t* p = g.data.data();
    uint32_t declared = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
                      | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);

    // Copy the trailing bytes — everything past the declared payload — into a
    // staging buffer so the header can be re-parsed without the payload.
    size_t trailing = g.data.size() - size_t(declared);
    std::vector<uint8_t> stage(g.data.size());
    std::memcpy(stage.data(), p, trailing);

    if (stage.size() >= 12) {
        size.width  = uint16_t((uint32_t(stage[8])  << 8) | stage[9]);
        size.height = uint16_t((uint32_t(stage[10]) << 8) | stage[11]);
    }
    return size;
}

// Scratch label for graphic types outside the well-known set.
static const char* g_type_label;

const char* sbix_graphic_type_name(const SbixGlyphData& g) {
    Tag t = g.graphic_type;
    if (t == Tag::from_chars("png ")) return "png";
    if (t == Tag::from_chars("jpg ")) return "jpeg";
    if (t == Tag::from_chars("tiff")) return "tiff";
    if (t == Tag::from_chars("dupe")) return "dupe";

    // Unrecognized type: spell the four tag bytes into a scratch label.
    char label[5];
    label[0] = char((t.value >> 24) & 0xFF);
    label[1] = char((t.value >> 16) & 0xFF);
    label[2] = char((t.value >> 8)  & 0xFF);
    label[3] = char( t.value        & 0xFF);
    label[4] = '\0';
    g_type_label = label;
    return g_type_label;
}

} // namespace fontscope
