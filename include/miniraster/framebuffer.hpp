#pragma once

#include "miniraster/math.hpp"
#include <limits>
#include <string>
#include <vector>

namespace mr {

class Framebuffer {
public:
    Framebuffer(int width, int height);
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    void clear(Vec3 color = {}, float depth = std::numeric_limits<float>::infinity());
    Vec3& color(int x, int y);
    const Vec3& color(int x, int y) const;
    float& depth(int x, int y);
    float depth(int x, int y) const;
    std::vector<Vec3>& colors() noexcept { return colors_; }
    const std::vector<Vec3>& colors() const noexcept { return colors_; }
    std::vector<float>& depths() noexcept { return depths_; }
    const std::vector<float>& depths() const noexcept { return depths_; }
    // Linear RGB is converted to 8-bit sRGB for display; path is UTF-8.
    void write_ppm(const std::string& path) const;
private:
    std::size_t index(int x, int y) const;
    int width_, height_;
    std::vector<Vec3> colors_;
    std::vector<float> depths_;
};

} // namespace mr
