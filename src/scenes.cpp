#include "miniraster/scenes.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <stdexcept>

namespace mr {
namespace {
Vertex vertex(Vec3 p,Vec3 n,Vec2 uv,Vec3 color={1,1,1}) { return {p,n,uv,color}; }
void quad(Scene& s,Vec3 a,Vec3 b,Vec3 c,Vec3 d,Vec3 n,Vec3 color={1,1,1}) {
    const auto va=vertex(a,n,{0,1},color),vb=vertex(b,n,{1,1},color);
    const auto vc=vertex(c,n,{1,0},color),vd=vertex(d,n,{0,0},color);
    s.triangles.push_back({va,vb,vc}); s.triangles.push_back({va,vc,vd});
}
void cube(Scene& s,Vec3 center,float radius,Vec3 color) {
    const auto point=[&](float x,float y,float z){return center+Vec3{x,y,z}*radius;};
    quad(s,point(-1,-1,1),point(1,-1,1),point(1,1,1),point(-1,1,1),{0,0,1},color);
    quad(s,point(1,-1,-1),point(-1,-1,-1),point(-1,1,-1),point(1,1,-1),{0,0,-1},color);
    quad(s,point(1,-1,1),point(1,-1,-1),point(1,1,-1),point(1,1,1),{1,0,0},color);
    quad(s,point(-1,-1,-1),point(-1,-1,1),point(-1,1,1),point(-1,1,-1),{-1,0,0},color);
    quad(s,point(-1,1,1),point(1,1,1),point(1,1,-1),point(-1,1,-1),{0,1,0},color);
    quad(s,point(-1,-1,-1),point(1,-1,-1),point(1,-1,1),point(-1,-1,1),{0,-1,0},color);
}
float random_unit(std::mt19937& rng) { return static_cast<float>(rng()>>8)/16777216.0f; }
}
const char* scene_names() noexcept { return "triangle, cube, perspective, nearclip, small, overdraw"; }
Scene make_scene(const std::string& name,float aspect,int complexity,std::uint32_t seed,float angle) {
    if (complexity<1||complexity>200000) throw std::invalid_argument("complexity must be in [1,200000]");
    Scene scene; scene.model=rotation_y(angle);
    scene.projection=perspective(0.9599311f,aspect,0.1f,100.0f);
    scene.camera_position={0,0,3}; scene.view=look_at(scene.camera_position,{0,0,0},{0,1,0});
    if (name=="triangle") {
        scene.triangles.push_back({vertex({-0.9f,-0.7f,0},{0,0,1},{0,1},{1,0,0}),
                                   vertex({0.9f,-0.7f,0},{0,0,1},{1,1},{0,1,0}),
                                   vertex({0,0.9f,0},{0,0,1},{0.5f,0},{0,0,1})});
    } else if (name=="cube") {
        scene.camera_position={3.6f,2.8f,4.5f};
        scene.view=look_at(scene.camera_position,{0,-0.1f,0},{0,1,0});
        quad(scene,{-3,-1,-3},{3,-1,-3},{3,-1,3},{-3,-1,3},{0,1,0},{0.65f,0.72f,0.8f});
        cube(scene,{-0.65f,-0.25f,0.1f},0.75f,{0.5f,0.8f,1});
        cube(scene,{1.0f,-0.52f,-0.7f},0.48f,{1,0.55f,0.25f});
    } else if (name=="perspective") {
        scene.camera_position={0,1.6f,3.8f};
        scene.view=look_at(scene.camera_position,{0,-0.3f,-1.4f},{0,1,0});
        quad(scene,{-2,-0.6f,1},{2,-0.6f,1},{2,-0.6f,-5},{-2,-0.6f,-5},{0,1,0});
    } else if (name=="nearclip") {
        cube(scene,{0.6f,-0.3f,-0.8f},0.55f,{0.3f,0.8f,1});
        scene.triangles.push_back({vertex({-0.6f,-0.5f,2.0f},{0,0,1},{0,1},{1,0.25f,0.2f}),
                                   vertex({0.6f,-0.5f,2.0f},{0,0,1},{1,1},{0.2f,1,0.25f}),
                                   vertex({0,0.03f,3.04f},{0,0,1},{0.5f,0},{0.3f,0.4f,1})});
    } else if (name=="small") {
        std::mt19937 rng(seed);
        const int columns=static_cast<int>(std::ceil(std::sqrt(static_cast<double>(complexity))));
        const float step=2.8f/columns;
        scene.triangles.reserve(static_cast<std::size_t>(complexity));
        for (int i=0;i<complexity;++i) {
            const float x=-1.4f+(i%columns+0.08f)*step,y=-1.4f+(i/columns+0.08f)*step;
            const Vec3 color{0.15f+0.85f*random_unit(rng),0.15f+0.85f*random_unit(rng),0.15f+0.85f*random_unit(rng)};
            scene.triangles.push_back({vertex({x,y,0},{0,0,1},{0,0},color),
                                       vertex({x+0.82f*step,y,0},{0,0,1},{1,0},color),
                                       vertex({x+0.35f*step,y+0.82f*step,0},{0,0,1},{0,1},color)});
        }
    } else if (name=="overdraw") {
        const int layers=std::clamp(complexity,2,256);
        // Far-to-near submission exposes shading work, rather than an early-z best case.
        for (int i=0;i<layers;++i) {
            const float t=static_cast<float>(i)/(layers-1),z=-0.5f+0.9f*t;
            const Vec3 color{0.15f+0.7f*t,0.65f-0.4f*t,0.85f-0.5f*t};
            scene.triangles.push_back({vertex({-1.4f,-1.2f,z},{0,0,1},{0,1},color),
                                       vertex({1.4f,-1.2f,z},{0,0,1},{1,1},color),
                                       vertex({0,1.3f,z},{0,0,1},{0.5f,0},color)});
        }
    } else throw std::invalid_argument(std::string("unknown scene; choose ")+scene_names());
    return scene;
}
PreparedScene prepare_scene(const Scene& scene) {
    const auto start=std::chrono::steady_clock::now();
    PreparedScene prepared; prepared.source_triangles=scene.triangles.size();
    prepared.triangles.reserve(scene.triangles.size());
    const Mat4 mvp=scene.projection*scene.view*scene.model;
    for (const auto& triangle:scene.triangles) {
        ClipTriangle clip{};
        for (std::size_t i=0;i<3;++i) {
            const auto& vertex=triangle[i];
            const Vec4 p{vertex.position.x,vertex.position.y,vertex.position.z,1};
            const Vec4 world=scene.model*p;
            const Vec4 normal=scene.model*Vec4{vertex.normal.x,vertex.normal.y,vertex.normal.z,0};
            clip[i]={mvp*p,vertex.uv,{normal.x,normal.y,normal.z},vertex.color,{world.x,world.y,world.z}};
        }
        auto pieces=clip_triangle(clip);
        prepared.triangles.insert(prepared.triangles.end(),pieces.begin(),pieces.end());
    }
    prepared.transform_clip_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return prepared;
}
} // namespace mr
