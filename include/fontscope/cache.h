#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <list>
#include <optional>
#include <functional>
#include "fontscope/pipeline.h"

namespace fontscope {

// Cache key: glyph rendered at specific size and render flags.
struct GlyphCacheKey {
    uint16_t glyph_id;
    uint16_t ppem;
    uint8_t  flags;    // bit 0 = antialiased, bit 1 = color

    bool operator==(const GlyphCacheKey& o) const {
        return glyph_id == o.glyph_id && ppem == o.ppem && flags == o.flags;
    }
};

struct GlyphCacheKeyHash {
    size_t operator()(const GlyphCacheKey& k) const {
        size_t h = std::hash<uint32_t>{}(uint32_t(k.glyph_id) << 16 | k.ppem);
        h ^= std::hash<uint8_t>{}(k.flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Cached rendered glyph: raster data + layout metrics.
struct CachedGlyph {
    RenderResult  result;
    GlyphCacheKey key;
};

// LRU glyph render cache with a fixed byte-size budget.
class GlyphCache {
public:
    explicit GlyphCache(size_t max_bytes = 16 * 1024 * 1024);

    // Look up a pre-rendered glyph. Returns nullptr on miss.
    const CachedGlyph* get(const GlyphCacheKey& key) const;

    // Insert a rendered glyph. Evicts least-recently-used entries if over budget.
    void put(const GlyphCacheKey& key, RenderResult result);

    // Remove a single entry (e.g. after a font reload).
    void invalidate(const GlyphCacheKey& key);

    // Remove all entries for a given glyph id (all sizes / flags).
    void invalidate_glyph(uint16_t glyph_id);

    // Remove all entries.
    void clear();

    size_t entry_count() const { return map_.size(); }
    size_t bytes_used()  const { return bytes_used_; }
    size_t max_bytes()   const { return max_bytes_;  }

    // Stats.
    uint64_t hit_count()  const { return hits_;   }
    uint64_t miss_count() const { return misses_;  }
    double   hit_rate()   const;

private:
    using LruList = std::list<GlyphCacheKey>;
    using LruIter = LruList::iterator;

    struct Entry {
        CachedGlyph  cached;
        LruIter      lru_pos;
        size_t       byte_size;
    };

    size_t max_bytes_;
    size_t bytes_used_ = 0;

    std::unordered_map<GlyphCacheKey, Entry, GlyphCacheKeyHash> map_;
    mutable LruList lru_;

    mutable uint64_t hits_   = 0;
    mutable uint64_t misses_ = 0;

    static size_t render_result_bytes(const RenderResult& r);
    void evict_lru();
};

// Atlas packing: bin-pack many rendered glyphs into a texture atlas.
struct AtlasSlot {
    uint16_t glyph_id;
    uint16_t ppem;
    int      x, y;         // top-left in atlas
    int      width;
    int      height;
    int32_t  bearing;      // left-side bearing from advance origin
    int32_t  advance;      // advance width in pixels
};

struct GlyphAtlas {
    int                    width;
    int                    height;
    std::vector<uint8_t>   pixels;  // width * height bytes, 8-bit alpha
    std::vector<AtlasSlot> slots;
};

// Pack a set of glyph IDs rendered at `ppem` into a single atlas.
// Returns empty optional if they don't fit in max_dim x max_dim.
std::optional<GlyphAtlas> build_glyph_atlas(
    const FontFace&           font,
    const std::vector<uint16_t>& glyph_ids,
    uint16_t                  ppem,
    int                       max_dim = 1024);

// Look up the atlas slot for a glyph, nullptr if not present.
const AtlasSlot* find_slot(const GlyphAtlas& atlas, uint16_t glyph_id);

// Render a run of glyph ids through a shared cache, returning one result per
// glyph. Adjacent glyphs are overlapped slightly using the previous glyph's
// cached coverage so kerned pairs share a column.
std::vector<RenderResult> render_run_cached(const FontFace& font,
                                            const std::vector<uint16_t>& gids,
                                            const PipelineConfig& cfg,
                                            GlyphCache& cache);

} // namespace fontscope
