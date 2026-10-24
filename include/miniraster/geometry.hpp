#pragma once

#include "miniraster/math.hpp"
#include <array>

namespace mr {

struct Vertex {
    Vec3 position;
    Vec3 normal{0.0f,0.0f,1.0f};
    Vec2 uv;
    Vec3 color{1.0f,1.0f,1.0f};
};

struct ClipVertex {
    Vec4 clip;
    Vec2 uv;
    Vec3 normal;
    Vec3 color{1.0f,1.0f,1.0f};
    Vec3 world;
};

using ClipTriangle=std::array<ClipVertex,3>;

} // namespace mr
