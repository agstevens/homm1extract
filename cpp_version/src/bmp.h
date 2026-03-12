#pragma once
#include "image.h"
#include "palette.h"
#include <vector>

namespace homm1 {

Image decode_bmp(const std::vector<uint8_t>& raw, const Palette& pal);

} // namespace homm1