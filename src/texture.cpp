#include "miniraster/texture.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mr {
namespace {
std::size_t texture_count(int w, int h) {
    if (w <= 0 || h <= 0) throw std::invalid_argument("texture dimensions must be positive");
    const auto a = static_cast<std::size_t>(w), b = static_cast<std::size_t>(h);
    if (a > std::numeric_limits<std::size_t>::max() / b) throw std::length_error("texture size overflows");
    return a * b;
}
Vec3 mix(Vec3 a, Vec3 b, float t) {
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
}
}
Texture::Texture(int width, int height, std::vector<Vec3> pixels)
    : width_(width), height_(height), pixels_(std::move(pixels)) {
    if (pixels_.size() != texture_count(width, height)) throw std::invalid_argument("texture pixel count mismatch");
}
Texture Texture::checkerboard(int width, int height, int cells) {
    if (cells <= 0) throw std::invalid_argument("checker cells must be positive");
    std::vector<Vec3> pixels(texture_count(width, height));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto cx = static_cast<long long>(x) * cells / width;
        const auto cy = static_cast<long long>(y) * cells / height;
        pixels[static_cast<std::size_t>(y)*width+x] = ((cx+cy)&1) ? Vec3{0.05f,0.15f,0.28f} : Vec3{0.95f,0.85f,0.6f};
    }
    return Texture(width, height, std::move(pixels));
}
Vec3 Texture::sample(Vec2 uv, Filter filter) const {
    if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) return {};
    const double x = std::clamp(static_cast<double>(uv.x), 0.0, 1.0) * (width_-1);
    const double y = std::clamp(static_cast<double>(uv.y), 0.0, 1.0) * (height_-1);
    const auto at = [this](int tx, int ty) { return pixels_[static_cast<std::size_t>(ty)*width_+tx]; };
    if (filter == Filter::Nearest) return at(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0+1,width_-1), y1 = std::min(y0+1,height_-1);
    return mix(mix(at(x0,y0),at(x1,y0),static_cast<float>(x-x0)),mix(at(x0,y1),at(x1,y1),static_cast<float>(x-x0)),static_cast<float>(y-y0));
}
} // namespace mr
