#include "miniraster/framebuffer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace mr {
namespace {
std::size_t pixel_count(int w, int h) {
    if (w <= 0 || h <= 0) throw std::invalid_argument("framebuffer dimensions must be positive");
    const auto width = static_cast<std::size_t>(w), height = static_cast<std::size_t>(h);
    if (width > std::numeric_limits<std::size_t>::max() / height)
        throw std::length_error("framebuffer pixel count overflows");
    return width * height;
}
unsigned char display_byte(float value) {
    const float c = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    const float srgb = c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<unsigned char>(std::lround(srgb * 255.0f));
}
}
Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height), colors_(pixel_count(width, height)),
      depths_(colors_.size(), std::numeric_limits<float>::infinity()) {}
void Framebuffer::clear(Vec3 color, float depth) {
    std::fill(colors_.begin(), colors_.end(), color);
    std::fill(depths_.begin(), depths_.end(), depth);
}
std::size_t Framebuffer::index(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) throw std::out_of_range("pixel out of bounds");
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}
Vec3& Framebuffer::color(int x, int y) { return colors_[index(x, y)]; }
const Vec3& Framebuffer::color(int x, int y) const { return colors_[index(x, y)]; }
float& Framebuffer::depth(int x, int y) { return depths_[index(x, y)]; }
float Framebuffer::depth(int x, int y) const { return depths_[index(x, y)]; }
void Framebuffer::write_ppm(const std::string& path) const {
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary);
    if (!out) throw std::runtime_error("cannot open PPM output: " + path);
    out << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for (const Vec3 c : colors_) {
        const unsigned char rgb[] = {display_byte(c.x), display_byte(c.y), display_byte(c.z)};
        out.write(reinterpret_cast<const char*>(rgb), 3);
    }
    out.flush();
    if (!out) throw std::runtime_error("failed writing PPM output: " + path);
}
} // namespace mr
