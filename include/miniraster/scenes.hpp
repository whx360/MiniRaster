#pragma once
#include "miniraster/clipper.hpp"
#include "miniraster/texture.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mr {
struct Scene {
    std::vector<std::array<Vertex,3>> triangles;
    Mat4 model=Mat4::identity(), view=Mat4::identity(), projection=Mat4::identity();
    Texture texture=Texture::checkerboard(128,128,12);
    Vec3 camera_position{0,0,3};
};
struct PreparedScene {
    std::vector<ClipTriangle> triangles;
    double transform_clip_ms=0;
    std::size_t source_triangles=0;
};
Scene make_scene(const std::string& name,float aspect,int complexity=4096,
                 std::uint32_t seed=7,float angle=0.0f);
// Current scene models are rigid transforms. Nonuniform scale needs inverse-transpose normals.
PreparedScene prepare_scene(const Scene& scene);
const char* scene_names() noexcept;
} // namespace mr
