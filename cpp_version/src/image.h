#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace homm1 {

struct Color { uint8_t r, g, b; };

struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels; // RGBA, row-major

    Image() = default;
    Image(int w, int h);

    void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void set_pixel_rgb(int x, int y, const Color& c);
};

void save_png(const Image& img, const std::string& path);

} // namespace homm1