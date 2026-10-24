#include "miniraster/rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const std::string& message) { if (!condition) throw std::runtime_error(message); }
void close(float actual,float expected,const std::string& message) { require(std::abs(actual-expected)<2e-6f,message); }
template<class Function> void throws(Function function,const std::string& message) {
    bool caught=false; try { function(); } catch (const std::exception&) { caught=true; } require(caught,message);
}
mr::ClipVertex vertex(float x,float y,int width,int height,mr::Vec3 color={1,1,1},float z=0,float w=1) {
    mr::ClipVertex v;
    v.clip={(2*x/width-1)*w,(1-2*y/height)*w,z*w,w}; v.color=color; v.normal={0,0,1}; v.world={x/width,y/height,0};
    return v;
}
std::vector<mr::ClipTriangle> square(int width,int height,mr::Vec3 color,float z=0) {
    const auto a=vertex(0,0,width,height,color,z), b=vertex(static_cast<float>(width),0,width,height,color,z);
    const auto c=vertex(static_cast<float>(width),static_cast<float>(height),width,height,color,z),d=vertex(0,static_cast<float>(height),width,height,color,z);
    return {{a,b,c},{a,c,d}};
}
bool identical(const mr::Framebuffer& a,const mr::Framebuffer& b) {
    if (a.colors().size()!=b.colors().size()) return false;
    for (std::size_t i=0;i<a.colors().size();++i) {
        const auto x=a.colors()[i],y=b.colors()[i];
        if (x.x!=y.x||x.y!=y.y||x.z!=y.z||a.depths()[i]!=b.depths()[i]) return false;
    }
    return true;
}
void shared_edges() {
    mr::Rasterizer rasterizer;
    for (int reverse=0;reverse<2;++reverse) {
        mr::Framebuffer fb(8,8); std::vector<std::uint32_t> coverage(64,0);
        auto triangles=square(8,8,{1,0,0});
        if (reverse) for (auto& triangle:triangles) std::swap(triangle[1],triangle[2]);
        mr::RenderOptions o; o.coverage=&coverage;
        rasterizer.render(triangles,fb,o);
        require(std::all_of(coverage.begin(),coverage.end(),[](std::uint32_t value){ return value==1; }),"square shared edge: crack or overlap");
    }
    // Two differently colored equal-depth primitives preserve submission order on ties.
    mr::Framebuffer fb(8,8);
    auto triangles=square(8,8,{1,0,0}); const auto second=square(8,8,{0,1,0});
    triangles.insert(triangles.end(),second.begin(),second.end()); rasterizer.render(triangles,fb);
    close(fb.color(4,4).x,1,"equal depth must retain first primitive");
}
void occlusion_and_interpolation() {
    mr::Rasterizer rasterizer; mr::Framebuffer fb(4,4);
    auto triangles=square(4,4,{1,0,0},0.7f); const auto near=square(4,4,{0,1,0},-0.7f);
    triangles.insert(triangles.end(),near.begin(),near.end()); rasterizer.render(triangles,fb);
    close(fb.color(1,1).y,1,"near triangle does not win depth test"); close(fb.depth(1,1),0.15f,"NDC depth mapping");
    std::reverse(triangles.begin(),triangles.end()); mr::Framebuffer reversed(4,4); rasterizer.render(triangles,reversed);
    require(identical(fb,reversed),"opaque visibility depends on submission order");

    mr::ClipTriangle triangle{{vertex(0,0,4,4,{1,0,0},-0.8f,1),vertex(4,0,4,4,{0,1,0},0.2f,2),vertex(0,4,4,4,{0,0,1},0.8f,4)}};
    fb.clear(); rasterizer.render({triangle},fb);
    close(fb.color(0,0).x,8.0f/9.0f,"perspective-correct red weight");
    close(fb.color(0,0).y,2.0f/27.0f,"perspective-correct green weight");
    close(fb.color(0,0).z,1.0f/27.0f,"perspective-correct blue weight");
    close(fb.depth(0,0),0.2625f,"depth must use screen-linear weights, not 1/w weights");
    auto scaled=triangle;
    const float factor=std::ldexp(1.0f,-40);
    for (auto& v:scaled) { v.clip.x*=factor;v.clip.y*=factor;v.clip.z*=factor;v.clip.w*=factor; }
    mr::Framebuffer homogeneous_scaled(4,4);rasterizer.render({scaled},homogeneous_scaled);
    require(identical(fb,homogeneous_scaled),"homogeneous positive scaling changed rendering");
    mr::RenderOptions affine; affine.perspective_correct=false; fb.clear(); rasterizer.render({triangle},fb,affine);
    close(fb.color(0,0).x,0.75f,"affine control red weight"); close(fb.color(0,0).y,0.125f,"affine control green weight");
    close(fb.depth(0,0),0.2625f,"affine control must not change depth");
    triangle[0].color=triangle[1].color=triangle[2].color={1,1,1};
    triangle[0].uv={0,0}; triangle[1].uv={1,0}; triangle[2].uv={0,1};
    const mr::Texture gradient(2,2,{{0,0,0},{1,0,0},{0,1,0},{1,1,0}});
    mr::RenderOptions textured; textured.shader=mr::ShaderMode::Textured; textured.texture=&gradient;
    fb.clear(); rasterizer.render({triangle},fb,textured);
    close(fb.color(0,0).x,2.0f/27.0f,"perspective texture u"); close(fb.color(0,0).y,1.0f/27.0f,"perspective texture v");
}
void texture_boundaries() {
    const mr::Texture texture(2,2,{{1,0,0},{0,1,0},{0,0,1},{1,1,1}});
    const auto middle=texture.sample({0.5f,0.5f}); close(middle.x,0.5f,"bilinear middle R"); close(middle.y,0.5f,"bilinear middle G"); close(middle.z,0.5f,"bilinear middle B");
    close(texture.sample({-10,-10}).x,1,"texture clamp lower"); close(texture.sample({10,10}).y,1,"texture clamp upper");
    close(texture.sample({1,0},mr::Filter::Nearest).y,1,"nearest right boundary");
    const mr::Texture singleton(1,1,{{0.2f,0.3f,0.4f}}); close(singleton.sample({0.7f,0.9f}).z,0.4f,"singleton bilinear");
    close(texture.sample({std::numeric_limits<float>::quiet_NaN(),0}).x,0,"NaN texture coordinate policy");
    throws([] { mr::Texture(0,2,{}); },"zero texture dimension accepted");
    throws([] { mr::Texture(2,2,{{1,1,1}}); },"mismatched texture data accepted");
    throws([] { mr::Texture::checkerboard(2,2,0); },"zero checker cells accepted");
}
void tile_parity() {
    constexpr int width=37,height=29;
    std::mt19937 rng(20261023); std::uniform_real_distribution<float> unit(0,1);
    std::vector<mr::ClipTriangle> triangles;
    for (int i=0;i<80;++i) {
        mr::ClipTriangle triangle;
        for (auto& v:triangle) {
            v=vertex(unit(rng)*width,unit(rng)*height,width,height,{unit(rng),unit(rng),unit(rng)},unit(rng)*1.8f-0.9f,0.5f+unit(rng)*3);
            v.uv={unit(rng),unit(rng)}; v.normal={unit(rng),unit(rng),0.5f};
        }
        triangles.push_back(triangle);
    }
    const auto checker=mr::Texture::checkerboard(32,32,5);
    mr::ThreadPool pool(3); mr::Rasterizer rasterizer;
    for (const auto shader:{mr::ShaderMode::VertexColor,mr::ShaderMode::Textured,mr::ShaderMode::BlinnPhong}) {
        mr::Framebuffer reference(width,height); std::vector<std::uint32_t> reference_coverage(width*height,0);
        mr::RenderOptions options; options.coverage=&reference_coverage; options.shader=shader; options.texture=&checker;
        const auto reference_stats=rasterizer.render(triangles,reference,options);
        for (const int tile:{1,7,16,100}) for (const auto mode:{mr::RenderMode::TileSerial,mr::RenderMode::TileParallel}) {
            mr::Framebuffer candidate(width,height); std::vector<std::uint32_t> candidate_coverage(width*height,0);
            options.mode=mode; options.tile_size=tile; options.coverage=&candidate_coverage;
            const auto stats=rasterizer.render(triangles,candidate,options,&pool);
            require(identical(reference,candidate),"reference/tile pixel or depth mismatch");
            require(reference_coverage==candidate_coverage,"reference/tile coverage mismatch");
            require(reference_stats.shaded_fragments==stats.shaded_fragments,"shaded fragment count mismatch");
            require(stats.total_ms>=0&&stats.tile_references>0&&stats.auxiliary_bytes>0,"invalid raster statistics");
        }
    }
}
void invalid_and_degenerate() {
    mr::Rasterizer rasterizer; mr::Framebuffer fb(8,8); const auto original=fb;
    const auto v=vertex(0,0,8,8);
    mr::ClipTriangle bad{{v,v,v}};
    auto triangles=std::vector<mr::ClipTriangle>{bad};
    bad[1]=vertex(8,0,8,8);bad[2]=vertex(0,8,8,8);bad[0].clip.x=std::numeric_limits<float>::quiet_NaN();triangles.push_back(bad);
    bad[0]=v;bad[0].clip.w=0;triangles.push_back(bad);
    bad[0]=v;bad[0].clip.x=1e30f;triangles.push_back(bad);
    const auto stats=rasterizer.render(triangles,fb);
    require(stats.prepared_triangles==0&&identical(fb,original),"invalid triangle changed framebuffer");
    rasterizer.render({},fb); require(identical(fb,original),"empty scene changed framebuffer");
    throws([] { mr::Framebuffer(0,1); },"zero framebuffer dimension accepted");
    throws([&] { fb.color(-1,0); },"framebuffer bounds check missing");
    mr::RenderOptions options; options.tile_size=0;
    throws([&] { rasterizer.render({},fb,options); },"zero tile size accepted");
    options.tile_size=16;options.mode=mr::RenderMode::TileParallel;
    throws([&] { rasterizer.render({},fb,options); },"parallel mode accepted missing pool");
    std::vector<std::uint32_t> wrong_size(3); options.mode=mr::RenderMode::Reference;options.coverage=&wrong_size;
    throws([&] { rasterizer.render({},fb,options); },"bad coverage buffer accepted");
}
}
int main() {
    try { shared_edges();occlusion_and_interpolation();texture_boundaries();tile_parity();invalid_and_degenerate();
        std::cout << "rasterizer tests passed (shared edge, depth, perspective, texture, tile/thread parity, invalid input)\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "rasterizer test failure: " << error.what() << '\n'; return 1; }
}
