#include "til.h"
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace homm1 {

namespace fs = std::filesystem;

static uint16_t r_u16(const uint8_t* d, size_t& p) {
    uint16_t v; std::memcpy(&v, d + p, 2); p += 2; return v;
}

TilFile decode_til(const std::vector<uint8_t>& raw, const Palette& pal) {
    TilFile result;
    if (raw.size() < 6)
        throw std::runtime_error("TIL file too small");

    const uint8_t* d = raw.data();
    size_t pos = 0;

    const uint16_t n_tiles = r_u16(d, pos);
    result.tile_width      = r_u16(d, pos);
    result.tile_height     = r_u16(d, pos);

    if (result.tile_width == 0 || result.tile_height == 0) return result;

    const size_t tile_sz = static_cast<size_t>(result.tile_width) * result.tile_height;

    result.tiles.reserve(n_tiles);
    for (uint16_t i = 0; i < n_tiles; ++i) {
        if (pos + tile_sz > raw.size())
            break; // truncated

        Image img(result.tile_width, result.tile_height);
        for (int py = 0; py < result.tile_height; ++py) {
            for (int px = 0; px < result.tile_width; ++px) {
                const uint8_t ci = d[pos++];
                img.set_pixel_rgb(px, py, pal[ci]);
            }
        }
        result.tiles.push_back(std::move(img));
    }

    return result;
}

void save_til(const TilFile& til, const std::string& out_dir) {
    fs::create_directories(out_dir);
    for (size_t i = 0; i < til.tiles.size(); ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "%04zu.png", i);
        save_png(til.tiles[i], out_dir + "/" + name);
    }
}

} // namespace homm1
