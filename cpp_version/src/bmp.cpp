#include "bmp.h"
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace homm1 {

static uint16_t r_u16(const uint8_t* d, size_t& p) {
    uint16_t v; std::memcpy(&v, d + p, 2); p += 2; return v;
}

Image decode_bmp(const std::vector<uint8_t>& raw, const Palette& pal) {
    if (raw.size() < 6)
        throw std::runtime_error("HoMM BMP too small");

    const uint8_t* d = raw.data();
    size_t pos = 0;

    const uint8_t magic_hi = d[pos++];
    const uint8_t magic_lo = d[pos++];
    if (magic_hi != 0x21 || magic_lo != 0x00) {
        std::ostringstream ss;
        ss << "HoMM BMP bad magic: 0x"
           << std::hex << static_cast<int>(magic_hi)
           << " 0x" << static_cast<int>(magic_lo);
        throw std::runtime_error(ss.str());
    }

    const uint16_t width  = r_u16(d, pos);
    const uint16_t height = r_u16(d, pos);

    if (width == 0 || height == 0)
        throw std::runtime_error("HoMM BMP has zero dimensions");

    const size_t n_pixels = static_cast<size_t>(width) * height;
    if (pos + n_pixels > raw.size())
        throw std::runtime_error("HoMM BMP pixel data truncated");

    Image img(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint8_t ci = d[pos++];
            img.set_pixel_rgb(x, y, pal[ci]);
        }
    }

    return img;
}

} // namespace homm1
