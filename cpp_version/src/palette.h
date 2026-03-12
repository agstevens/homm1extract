#pragma once
#include "image.h"
#include <array>
#include <string>
#include <vector>

namespace homm1 {

using Palette = std::array<Color, 256>;

Palette load_palette(const std::vector<uint8_t>& data);
void save_palette_swatch(const Palette& pal, const std::string& path);

} // namespace homm1