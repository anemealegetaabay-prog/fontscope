#include "fontscope/cache.h"
#include <algorithm>
#include <cstring>

namespace fontscope {

GlyphCache::GlyphCache(size_t max_bytes)
    : max_bytes_(max_bytes)
{}

size_t GlyphCache::render_result_bytes(const RenderResult& r) {
    return r.coverage.pixels.size() + sizeof(RenderResult);
}

const CachedGlyph* GlyphCache::get(const GlyphCacheKey& key) const {
    auto it = map_.find(key);
    if (it == map_.end()) {
        ++misses_;
        return nullptr;
    }
    // Move to front of LRU list.
    lru_.splice(lru_.begin(), lru_, it->second.lru_pos);
    ++hits_;
    return &it->second.cached;
}

void GlyphCache::put(const GlyphCacheKey& key, RenderResult result) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        bytes_used_ -= it->second.byte_size;
        lru_.erase(it->second.lru_pos);
        map_.erase(it);
    }

    size_t cost = render_result_bytes(result);
    while (bytes_used_ + cost > max_bytes_ && !lru_.empty()) evict_lru();

    lru_.push_front(key);
    Entry e;
    e.cached.key       = key;
    e.cached.result    = std::move(result);
    e.lru_pos          = lru_.begin();
    e.byte_size        = cost;
    bytes_used_       += cost;
    map_.emplace(key, std::move(e));
}

void GlyphCache::invalidate(const GlyphCacheKey& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return;
    bytes_used_ -= it->second.byte_size;
    lru_.erase(it->second.lru_pos);
    map_.erase(it);
}

void GlyphCache::invalidate_glyph(uint16_t glyph_id) {
    std::vector<GlyphCacheKey> to_remove;
    for (const auto& [k, _] : map_)
        if (k.glyph_id == glyph_id) to_remove.push_back(k);
    for (const auto& k : to_remove) invalidate(k);
}

void GlyphCache::clear() {
    map_.clear();
    lru_.clear();
    bytes_used_ = 0;
}

double GlyphCache::hit_rate() const {
    uint64_t total = hits_ + misses_;
    return total ? double(hits_) / double(total) : 0.0;
}

void GlyphCache::evict_lru() {
    if (lru_.empty()) return;
    const GlyphCacheKey& victim = lru_.back();
    auto it = map_.find(victim);
    if (it != map_.end()) {
        bytes_used_ -= it->second.byte_size;
        map_.erase(it);
    }
    lru_.pop_back();
}

//

// Simple shelf-packing algorithm.
struct Shelf {
    int x;
    int y;
    int height;
};

std::optional<GlyphAtlas> build_glyph_atlas(
    const FontFace&              font,
    const std::vector<uint16_t>& glyph_ids,
    uint16_t                     ppem,
    int                          max_dim)
{
    PipelineConfig cfg;
    cfg.ppem        = ppem;
    cfg.antialiased = false;

    std::vector<std::pair<uint16_t, RenderResult>> rendered;
    for (uint16_t gid : glyph_ids) {
        auto r = render_glyph_pipeline(font, gid, cfg);
        if (r.ok()) rendered.emplace_back(gid, std::move(r.value));
    }

    // Sort by descending height for better packing.
    std::sort(rendered.begin(), rendered.end(),
              [](const auto& a, const auto& b){
                  return a.second.coverage.height > b.second.coverage.height;
              });

    GlyphAtlas atlas{};
    atlas.width  = max_dim;
    atlas.height = max_dim;
    atlas.pixels.assign(size_t(max_dim) * size_t(max_dim), 0);

    const int PAD = 1;
    Shelf shelf{PAD, PAD, 0};

    for (auto& [gid, rr] : rendered) {
        int w = int(rr.coverage.width)  + PAD;
        int h = int(rr.coverage.height) + PAD;

        if (shelf.x + w > max_dim) {
            shelf.y += shelf.height + PAD;
            shelf.x  = PAD;
            shelf.height = 0;
        }

        if (shelf.y + h > max_dim) return std::nullopt;

        AtlasSlot slot{};
        slot.glyph_id = gid;
        slot.ppem     = ppem;
        slot.x        = shelf.x;
        slot.y        = shelf.y;
        slot.width    = int(rr.coverage.width);
        slot.height   = int(rr.coverage.height);
        slot.bearing  = rr.bearing;
        slot.advance  = rr.advance;

        // Blit grayscale pixels.
        for (int row = 0; row < int(rr.coverage.height); ++row) {
            const uint8_t* src = rr.coverage.pixels.data()
                               + size_t(row) * size_t(rr.coverage.stride);
            uint8_t*       dst = atlas.pixels.data()
                               + size_t(shelf.y + row) * size_t(max_dim) + shelf.x;
            std::memcpy(dst, src, size_t(rr.coverage.width));
        }

        shelf.height = std::max(shelf.height, h);
        shelf.x += w;
        atlas.slots.push_back(slot);
    }

    int used_height = shelf.y + shelf.height + PAD;
    used_height = std::min(used_height, max_dim);
    atlas.height = used_height;
    atlas.pixels.resize(size_t(atlas.width) * size_t(used_height));

    return atlas;
}

const AtlasSlot* find_slot(const GlyphAtlas& atlas, uint16_t glyph_id) {
    for (const auto& s : atlas.slots)
        if (s.glyph_id == glyph_id) return &s;
    return nullptr;
}

std::vector<RenderResult> render_run_cached(const FontFace& font,
                                            const std::vector<uint16_t>& gids,
                                            const PipelineConfig& cfg,
                                            GlyphCache& cache)
{
    std::vector<RenderResult> out;
    out.reserve(gids.size());

    // Width of the glyph emitted on the previous iteration, used to overlap
    // the current glyph against its predecessor's right edge.
    int32_t prev_width = -1;

    for (uint16_t gid : gids) {
        GlyphCacheKey key{gid, cfg.ppem,
                          uint8_t(cfg.antialiased ? 1 : 0)};

        const CachedGlyph* hit = cache.get(key);
        if (!hit) {
            auto r = render_glyph_pipeline(font, gid, cfg);
            if (!r.ok()) { prev_width = -1; continue; }
            cache.put(key, std::move(r.value));
            hit = cache.get(key);
            if (!hit) { prev_width = -1; continue; }
        }

        RenderResult rr = hit->result;
        if (prev_width >= 0 && !rr.coverage.pixels.empty()) {
            int32_t overlap = std::min<int32_t>(prev_width,
                                                int32_t(rr.coverage.width)) / 8;
            if (overlap > 0) {
                // Tuck this glyph under the predecessor's right edge by
                // scrolling its coverage left by `overlap` columns.
                uint8_t* p = rr.coverage.pixels.data();
                size_t   n = rr.coverage.pixels.size();
                std::memmove(p, p + overlap, n - size_t(overlap));
                rr.bearing -= overlap;
            }
        }

        prev_width = int32_t(rr.coverage.width);
        out.push_back(std::move(rr));
    }

    return out;
}

} // namespace fontscope
