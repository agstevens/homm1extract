#pragma once
#include "image.h"
#include "palette.h"
#include <string>
#include <vector>

namespace homm1 {

struct TilFile {
    uint16_t tile_width  = 0;
    uint16_t tile_height = 0;
    std::vector<Image> tiles;
};

TilFile decode_til(const std::vector<uint8_t>& raw, const Palette& pal);
void save_til(const TilFile& til, const std::string& out_dir);

} // namespace homm1