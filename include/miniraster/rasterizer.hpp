#pragma once
#include "miniraster/geometry.hpp"
#include "miniraster/framebuffer.hpp"
#include "miniraster/texture.hpp"
#include "miniraster/thread_pool.hpp"
#include <cstdint>
#include <vector>

namespace mr {
enum class RenderMode { Reference, TileSerial, TileParallel };
enum class ShaderMode { VertexColor, Textured, BlinnPhong };
struct RenderOptions {
    RenderMode mode = RenderMode::Reference;
    int tile_size = 16;
    bool perspective_correct = true;
    ShaderMode shader = ShaderMode::VertexColor;
    const Texture* texture = nullptr;
    Filter filter = Filter::Bilinear;
    Vec3 camera_position{0,0,3};
    Vec3 light_direction{0.3f,0.5f,1.0f}; // from the surface toward the light
    // Optional diagnostic counter, pre-sized width*height and initialized by caller.
    // Counts covered samples before the depth test, so shared-edge tests detect overlap.
    std::vector<std::uint32_t>* coverage = nullptr;
};
struct RenderStats {
    double prepare_ms = 0, bin_ms = 0, raster_ms = 0, total_ms = 0;
    std::size_t input_triangles = 0, prepared_triangles = 0;
    std::size_t tile_references = 0, auxiliary_bytes = 0;
    std::uint64_t shaded_fragments = 0;
};
class Rasterizer {
public:
    // Input has already passed homogeneous clipping. Does not clear the framebuffer.
    // Three traversal modes use exactly the same coverage/interpolation/shader kernel.
    // Parallel mode needs a caller-owned, reusable pool; this call waits for completion.
    RenderStats render(const std::vector<ClipTriangle>& triangles, Framebuffer& target,
                       const RenderOptions& options = {}, ThreadPool* pool = nullptr) const;
};
} // namespace mr
