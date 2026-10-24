#pragma once
#include "miniraster/math.hpp"
#include <vector>

namespace mr {
enum class Filter { Nearest, Bilinear };
class Texture {
public:
    Texture(int width, int height, std::vector<Vec3> pixels);
    static Texture checkerboard(int width, int height, int cells = 8);
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    // Clamp addressing. UV (0,0) is the upper-left texel center and (1,1) lower-right.
    Vec3 sample(Vec2 uv, Filter filter = Filter::Bilinear) const;
private:
    int width_, height_;
    std::vector<Vec3> pixels_;
};
} // namespace mr
