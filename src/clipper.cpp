#include "miniraster/clipper.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mr {
namespace {

enum class Plane { Left,Right,Bottom,Top,Near,Far };

double distance(Vec4 p,Plane plane) {
    switch (plane) {
        case Plane::Left: return static_cast<double>(p.w)+p.x;
        case Plane::Right: return static_cast<double>(p.w)-p.x;
        case Plane::Bottom: return static_cast<double>(p.w)+p.y;
        case Plane::Top: return static_cast<double>(p.w)-p.y;
        case Plane::Near: return static_cast<double>(p.w)+p.z;
        case Plane::Far: return static_cast<double>(p.w)-p.z;
    }
    return 0.0;
}

float mix(float a,float b,double t) {
    return static_cast<float>((1.0-t)*static_cast<double>(a)+t*static_cast<double>(b));
}
Vec2 mix(Vec2 a,Vec2 b,double t) { return {mix(a.x,b.x,t),mix(a.y,b.y,t)}; }
Vec3 mix(Vec3 a,Vec3 b,double t) { return {mix(a.x,b.x,t),mix(a.y,b.y,t),mix(a.z,b.z,t)}; }
Vec4 mix(Vec4 a,Vec4 b,double t) {
    return {mix(a.x,b.x,t),mix(a.y,b.y,t),mix(a.z,b.z,t),mix(a.w,b.w,t)};
}

ClipVertex intersection(const ClipVertex& a,const ClipVertex& b,double da,double db,Plane plane) {
    // Opposite inside classifications guarantee da-db != 0 and t in [0,1].
    const double t=std::clamp(da/(da-db),0.0,1.0);
    ClipVertex v{mix(a.clip,b.clip,t),mix(a.uv,b.uv,t),mix(a.normal,b.normal,t),
                 mix(a.color,b.color,t),mix(a.world,b.world,t)};
    // Canonicalize the intersected plane after rounding to float.
    switch (plane) {
        case Plane::Left: v.clip.x=-v.clip.w; break;
        case Plane::Right: v.clip.x=v.clip.w; break;
        case Plane::Bottom: v.clip.y=-v.clip.w; break;
        case Plane::Top: v.clip.y=v.clip.w; break;
        case Plane::Near: v.clip.z=-v.clip.w; break;
        case Plane::Far: v.clip.z=v.clip.w; break;
    }
    return v;
}

bool same_position(const ClipVertex& a,const ClipVertex& b) {
    return a.clip.x==b.clip.x&&a.clip.y==b.clip.y&&a.clip.z==b.clip.z&&a.clip.w==b.clip.w;
}
void append_unique(std::vector<ClipVertex>& polygon,const ClipVertex& vertex) {
    if (polygon.empty()||!same_position(polygon.back(),vertex)) polygon.push_back(vertex);
}
bool drawable(const ClipTriangle& t) {
    for (const auto& v:t) if (!(v.clip.w>0.0f)) return false;
    const double ax=static_cast<double>(t[0].clip.x)/t[0].clip.w;
    const double ay=static_cast<double>(t[0].clip.y)/t[0].clip.w;
    const double bx=static_cast<double>(t[1].clip.x)/t[1].clip.w;
    const double by=static_cast<double>(t[1].clip.y)/t[1].clip.w;
    const double cx=static_cast<double>(t[2].clip.x)/t[2].clip.w;
    const double cy=static_cast<double>(t[2].clip.y)/t[2].clip.w;
    return (bx-ax)*(cy-ay)-(by-ay)*(cx-ax)!=0.0;
}

} // namespace

std::vector<ClipTriangle> clip_triangle(const ClipTriangle& triangle) {
    for (const auto& v:triangle) {
        if (!is_finite(v.clip)||!is_finite(v.uv)||!is_finite(v.normal)||
            !is_finite(v.color)||!is_finite(v.world)) {
            throw std::invalid_argument("clip_triangle: non-finite vertex or attribute");
        }
    }
    std::vector<ClipVertex> polygon(triangle.begin(),triangle.end());
    std::vector<ClipVertex> next;
    polygon.reserve(12); next.reserve(12);
    constexpr Plane planes[]{Plane::Left,Plane::Right,Plane::Bottom,Plane::Top,Plane::Near,Plane::Far};
    for (Plane plane:planes) {
        if (polygon.size()<3) return {};
        next.clear();
        ClipVertex previous=polygon.back();
        double previous_distance=distance(previous.clip,plane);
        bool previous_inside=previous_distance>=0.0;
        for (const auto& current:polygon) {
            const double current_distance=distance(current.clip,plane);
            const bool current_inside=current_distance>=0.0;
            if (previous_inside!=current_inside) {
                append_unique(next,intersection(previous,current,previous_distance,current_distance,plane));
            }
            if (current_inside) append_unique(next,current);
            previous=current; previous_distance=current_distance; previous_inside=current_inside;
        }
        if (next.size()>1&&same_position(next.front(),next.back())) next.pop_back();
        polygon.swap(next);
    }
    if (polygon.size()<3) return {};
    for (auto& v:polygon) {
        if (v.clip.w>=0.0f) {
            // Convex combinations are inside all processed half-spaces in exact
            // arithmetic. Clamp only the final float-rounding residual.
            v.clip.x=std::clamp(v.clip.x,-v.clip.w,v.clip.w);
            v.clip.y=std::clamp(v.clip.y,-v.clip.w,v.clip.w);
            v.clip.z=std::clamp(v.clip.z,-v.clip.w,v.clip.w);
        }
    }
    // The only point in the closed clip volume with w==0 is (0,0,0,0).
    // It has no projected position and contributes no new projective extent.
    // Remove it before choosing the fan anchor, or a singular first vertex
    // could incorrectly suppress otherwise drawable output triangles.
    polygon.erase(std::remove_if(polygon.begin(),polygon.end(),
                   [](const ClipVertex& v) { return !(v.clip.w>0.0f); }),polygon.end());
    if (polygon.size()<3) return {};
    std::vector<ClipTriangle> result;
    result.reserve(polygon.size()-2);
    for (std::size_t i=1;i+1<polygon.size();++i) {
        ClipTriangle t{polygon[0],polygon[i],polygon[i+1]};
        if (drawable(t)) result.push_back(t);
    }
    return result;
}

} // namespace mr
