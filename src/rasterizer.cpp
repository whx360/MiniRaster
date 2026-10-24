#include "miniraster/rasterizer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mr {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::int64_t subpixel = 256; // Eight fractional bits, exact shared-edge decisions.
constexpr double coordinate_limit = 1048576.0;
double milliseconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double,std::milli>(b-a).count();
}
bool finite(Vec2 a) { return std::isfinite(a.x)&&std::isfinite(a.y); }
bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
bool finite(Vec4 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::isfinite(a.w); }
struct ScreenVertex {
    std::int64_t x=0,y=0;
    double inv_w=0,depth=0;
    ClipVertex attribute;
};
struct PreparedTriangle {
    std::array<ScreenVertex,3> v;
    std::int64_t area=0;
    std::array<bool,3> inclusive{};
    int min_x=0,max_x=-1,min_y=0,max_y=-1;
};
std::int64_t edge(const ScreenVertex& a, const ScreenVertex& b, std::int64_t x, std::int64_t y) {
    return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
}
bool top_left(const ScreenVertex& a, const ScreenVertex& b) {
    const auto dx=b.x-a.x,dy=b.y-a.y;
    return dy<0 || (dy==0 && dx>0);
}
bool prepare(const ClipTriangle& triangle, int width, int height, PreparedTriangle& result) {
    for (int i=0;i<3;++i) {
        const auto& a=triangle[static_cast<std::size_t>(i)];
        if (!finite(a.clip)||!finite(a.uv)||!finite(a.normal)||!finite(a.color)||!finite(a.world)||a.clip.w<=0) return false;
        const double inverse=1.0/static_cast<double>(a.clip.w);
        const double x=(static_cast<double>(a.clip.x)*inverse+1.0)*0.5*width;
        const double y=(1.0-static_cast<double>(a.clip.y)*inverse)*0.5*height;
        const double z=static_cast<double>(a.clip.z)*inverse;
        // Finite, bounded fixed-point coordinates keep edge products inside int64.
        if (!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>coordinate_limit||std::abs(y)>coordinate_limit||z < -1.00001||z > 1.00001) return false;
        result.v[static_cast<std::size_t>(i)]={std::llround(x*subpixel),std::llround(y*subpixel),inverse,(std::clamp(z,-1.0,1.0)+1.0)*0.5,a};
    }
    result.area=edge(result.v[0],result.v[1],result.v[2].x,result.v[2].y);
    if (result.area==0) return false;
    if (result.area<0) { std::swap(result.v[1],result.v[2]); result.area=-result.area; }
    result.inclusive={top_left(result.v[1],result.v[2]),top_left(result.v[2],result.v[0]),top_left(result.v[0],result.v[1])};
    const auto min_x=std::min({result.v[0].x,result.v[1].x,result.v[2].x});
    const auto max_x=std::max({result.v[0].x,result.v[1].x,result.v[2].x});
    const auto min_y=std::min({result.v[0].y,result.v[1].y,result.v[2].y});
    const auto max_y=std::max({result.v[0].y,result.v[1].y,result.v[2].y});
    result.min_x=std::max(0,static_cast<int>(std::floor(static_cast<double>(min_x)/subpixel)));
    result.max_x=std::min(width-1,static_cast<int>(std::ceil(static_cast<double>(max_x)/subpixel))-1);
    result.min_y=std::max(0,static_cast<int>(std::floor(static_cast<double>(min_y)/subpixel)));
    result.max_y=std::min(height-1,static_cast<int>(std::ceil(static_cast<double>(max_y)/subpixel))-1);
    return result.min_x<=result.max_x && result.min_y<=result.max_y;
}
Vec3 weighted(Vec3 a,Vec3 b,Vec3 c,const std::array<double,3>& p) {
    return {static_cast<float>(a.x*p[0]+b.x*p[1]+c.x*p[2]),static_cast<float>(a.y*p[0]+b.y*p[1]+c.y*p[2]),static_cast<float>(a.z*p[0]+b.z*p[1]+c.z*p[2])};
}
Vec3 unit(Vec3 a) {
    const double length=std::sqrt(static_cast<double>(a.x)*a.x+static_cast<double>(a.y)*a.y+static_cast<double>(a.z)*a.z);
    if (length<=1e-12) return {};
    return {static_cast<float>(a.x/length),static_cast<float>(a.y/length),static_cast<float>(a.z/length)};
}
double scalar(Vec3 a,Vec3 b) { return static_cast<double>(a.x)*b.x+static_cast<double>(a.y)*b.y+static_cast<double>(a.z)*b.z; }
Vec3 shade(const PreparedTriangle& triangle,const std::array<double,3>& p,const RenderOptions& options) {
    const auto& a=triangle.v[0].attribute; const auto& b=triangle.v[1].attribute; const auto& c=triangle.v[2].attribute;
    Vec3 color=weighted(a.color,b.color,c.color,p);
    if (options.texture && options.shader!=ShaderMode::VertexColor) {
        const Vec2 uv{static_cast<float>(a.uv.x*p[0]+b.uv.x*p[1]+c.uv.x*p[2]),static_cast<float>(a.uv.y*p[0]+b.uv.y*p[1]+c.uv.y*p[2])};
        const Vec3 texel=options.texture->sample(uv,options.filter);
        color={color.x*texel.x,color.y*texel.y,color.z*texel.z};
    }
    if (options.shader==ShaderMode::BlinnPhong) {
        const Vec3 n=unit(weighted(a.normal,b.normal,c.normal,p));
        const Vec3 world=weighted(a.world,b.world,c.world,p);
        const Vec3 light=unit(options.light_direction);
        const Vec3 view=unit({options.camera_position.x-world.x,options.camera_position.y-world.y,options.camera_position.z-world.z});
        const Vec3 half=unit({light.x+view.x,light.y+view.y,light.z+view.z});
        const double diffuse=std::max(0.0,scalar(n,light));
        const double specular=diffuse>0 ? 0.2*std::pow(std::max(0.0,scalar(n,half)),32.0) : 0.0;
        const float illumination=static_cast<float>(0.15+0.85*diffuse);
        color={color.x*illumination+static_cast<float>(specular),color.y*illumination+static_cast<float>(specular),color.z*illumination+static_cast<float>(specular)};
    }
    return color;
}
std::uint64_t raster(const PreparedTriangle& triangle,Framebuffer& target,const RenderOptions& options,
                     int min_x,int max_x,int min_y,int max_y) {
    min_x=std::max(min_x,triangle.min_x); max_x=std::min(max_x,triangle.max_x);
    min_y=std::max(min_y,triangle.min_y); max_y=std::min(max_y,triangle.max_y);
    const double inverse_area=1.0/static_cast<double>(triangle.area);
    std::uint64_t shaded=0;
    auto& colors=target.colors(); auto& depths=target.depths();
    for (int y=min_y;y<=max_y;++y) for (int x=min_x;x<=max_x;++x) {
        const std::int64_t px=static_cast<std::int64_t>(x)*subpixel+subpixel/2;
        const std::int64_t py=static_cast<std::int64_t>(y)*subpixel+subpixel/2;
        const std::array<std::int64_t,3> edges{edge(triangle.v[1],triangle.v[2],px,py),edge(triangle.v[2],triangle.v[0],px,py),edge(triangle.v[0],triangle.v[1],px,py)};
        if (edges[0]<0 || (edges[0]==0&&!triangle.inclusive[0]) || edges[1]<0 || (edges[1]==0&&!triangle.inclusive[1]) || edges[2]<0 || (edges[2]==0&&!triangle.inclusive[2])) continue;
        const auto index=static_cast<std::size_t>(y)*target.width()+static_cast<std::size_t>(x);
        if (options.coverage) ++(*options.coverage)[index];
        std::array<double,3> p{edges[0]*inverse_area,edges[1]*inverse_area,edges[2]*inverse_area};
        const float z=static_cast<float>(p[0]*triangle.v[0].depth+p[1]*triangle.v[1].depth+p[2]*triangle.v[2].depth);
        if (!(z<depths[index])) continue;
        if (options.perspective_correct) {
            for (std::size_t i=0;i<3;++i) p[i]*=triangle.v[i].inv_w;
            const double denominator=p[0]+p[1]+p[2];
            if (!std::isfinite(denominator)||denominator<=0) continue;
            for (auto& value:p) value/=denominator;
        }
        colors[index]=shade(triangle,p,options); depths[index]=z; ++shaded;
    }
    return shaded;
}
}

RenderStats Rasterizer::render(const std::vector<ClipTriangle>& triangles,Framebuffer& target,
                              const RenderOptions& options,ThreadPool* pool) const {
    const auto begin=Clock::now();
    if (options.tile_size<=0) throw std::invalid_argument("tile size must be positive");
    if (options.mode==RenderMode::TileParallel&&!pool) throw std::invalid_argument("parallel rendering requires a pool");
    if (options.coverage&&options.coverage->size()!=target.colors().size()) throw std::invalid_argument("coverage buffer size mismatch");
    if (!finite(options.camera_position)||!finite(options.light_direction)) throw std::invalid_argument("non-finite lighting option");
    RenderStats stats; stats.input_triangles=triangles.size();
    std::vector<PreparedTriangle> prepared;
    prepared.reserve(triangles.size());
    for (const auto& triangle:triangles) { PreparedTriangle p; if (prepare(triangle,target.width(),target.height(),p)) prepared.push_back(p); }
    stats.prepared_triangles=prepared.size(); stats.auxiliary_bytes=prepared.capacity()*sizeof(PreparedTriangle);
    const auto prepare_end=Clock::now(); stats.prepare_ms=milliseconds(begin,prepare_end);
    if (options.mode==RenderMode::Reference) {
        for (const auto& triangle:prepared) stats.shaded_fragments+=raster(triangle,target,options,0,target.width()-1,0,target.height()-1);
        const auto end=Clock::now(); stats.raster_ms=milliseconds(prepare_end,end); stats.total_ms=milliseconds(begin,end); return stats;
    }
    const int columns=(target.width()-1)/options.tile_size+1;
    const int rows=(target.height()-1)/options.tile_size+1;
    const auto tile_count=static_cast<std::size_t>(columns)*static_cast<std::size_t>(rows);
    std::vector<std::vector<std::size_t>> bins(tile_count);
    for (std::size_t i=0;i<prepared.size();++i) {
        const auto& p=prepared[i];
        for (int ty=p.min_y/options.tile_size;ty<=p.max_y/options.tile_size;++ty)
            for (int tx=p.min_x/options.tile_size;tx<=p.max_x/options.tile_size;++tx) {
                bins[static_cast<std::size_t>(ty)*columns+tx].push_back(i); ++stats.tile_references;
            }
    }
    std::vector<std::uint64_t> counts(tile_count,0);
    stats.auxiliary_bytes+=bins.capacity()*sizeof(bins[0])+counts.capacity()*sizeof(counts[0]);
    for (const auto& bin:bins) stats.auxiliary_bytes+=bin.capacity()*sizeof(std::size_t);
    const auto bin_end=Clock::now(); stats.bin_ms=milliseconds(prepare_end,bin_end);
    const auto draw_tile=[&](std::size_t id) {
        const int x=static_cast<int>(id%columns)*options.tile_size,y=static_cast<int>(id/columns)*options.tile_size;
        // Avoid x+tile_size overflow even when an oversized tile was requested.
        const int max_x=x+std::min(options.tile_size-1,target.width()-1-x);
        const int max_y=y+std::min(options.tile_size-1,target.height()-1-y);
        for (const auto i:bins[id]) counts[id]+=raster(prepared[i],target,options,x,max_x,y,max_y);
    };
    if (options.mode==RenderMode::TileSerial) {
        for (std::size_t id=0;id<tile_count;++id) draw_tile(id);
    } else {
        // On allocation/enqueue failure, finish already submitted tasks before local data dies.
        try {
            for (std::size_t id=0;id<tile_count;++id) if (!bins[id].empty()) pool->enqueue([&,id] { draw_tile(id); });
        } catch (...) { const auto error=std::current_exception(); try { pool->wait(); } catch (...) {} std::rethrow_exception(error); }
        pool->wait();
    }
    for (const auto count:counts) stats.shaded_fragments+=count;
    const auto end=Clock::now(); stats.raster_ms=milliseconds(bin_end,end); stats.total_ms=milliseconds(begin,end);
    return stats;
}
} // namespace mr
