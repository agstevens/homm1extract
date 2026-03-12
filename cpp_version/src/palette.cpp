#include "palette.h"
#include "image.h"
#include <stdexcept>

namespace homm1 {

Palette load_palette(const std::vector<uint8_t>& data) {
    if (data.size() < 768)
        throw std::runtime_error("PAL data too small: need 768 bytes, got "
                                 + std::to_string(data.size()));
    Palette pal;
    for (int i = 0; i < 256; ++i) {
        // Each channel is 0-63; multiply by 4 to get 0-255.
        pal[i].r = static_cast<uint8_t>(std::min(data[i * 3 + 0] * 4, 255));
        pal[i].g = static_cast<uint8_t>(std::min(data[i * 3 + 1] * 4, 255));
        pal[i].b = static_cast<uint8_t>(std::min(data[i * 3 + 2] * 4, 255));
    }
    return pal;
}

void save_palette_swatch(const Palette& pal, const std::string& path) {
    if (path.empty()) return;
    // 256 wide, 32 tall: each column is one palette entry.
    Image img(256, 32);
    for (int i = 0; i < 256; ++i) {
        for (int y = 0; y < 32; ++y) {
            img.set_pixel(i, y, pal[i].r, pal[i].g, pal[i].b, 255);
        }
    }
    save_png(img, path);
}

} // namespace homm1
