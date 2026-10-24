#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace mr {

struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Vec4 { float x{}, y{}, z{}, w{}; };

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x+b.x, a.y+b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x-b.x, a.y-b.y}; }
inline Vec2 operator-(Vec2 a) { return {-a.x,-a.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x*s,a.y*s}; }
inline Vec2 operator*(float s, Vec2 a) { return a*s; }
inline Vec2 operator/(Vec2 a, float s) { return {a.x/s,a.y/s}; }
inline float dot(Vec2 a, Vec2 b) { return a.x*b.x+a.y*b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x*b.y-a.y*b.x; }
inline bool is_finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y); }
inline float length(Vec2 v) { return std::hypot(v.x,v.y); }

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x,-a.y,-a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline Vec3 operator*(float s, Vec3 a) { return a*s; }
inline Vec3 operator/(Vec3 a, float s) { return {a.x/s,a.y/s,a.z/s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool is_finite(Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
inline float length(Vec3 v) { return std::hypot(v.x,v.y,v.z); }

inline Vec4 operator+(Vec4 a, Vec4 b) { return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w}; }
inline Vec4 operator-(Vec4 a, Vec4 b) { return {a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w}; }
inline Vec4 operator-(Vec4 a) { return {-a.x,-a.y,-a.z,-a.w}; }
inline Vec4 operator*(Vec4 a, float s) { return {a.x*s,a.y*s,a.z*s,a.w*s}; }
inline Vec4 operator*(float s, Vec4 a) { return a*s; }
inline Vec4 operator/(Vec4 a, float s) { return {a.x/s,a.y/s,a.z/s,a.w/s}; }
inline float dot(Vec4 a, Vec4 b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
inline bool is_finite(Vec4 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w);
}
inline float length(Vec4 v) { return std::hypot(std::hypot(v.x,v.y),std::hypot(v.z,v.w)); }

// Compute the norm in double: a finite direction can exceed float's norm range.
// Normalization remains defined for both subnormal and very large inputs.
inline Vec2 normalize(Vec2 v) {
    if (!is_finite(v)) throw std::domain_error("normalize: non-finite vector");
    const double n=std::hypot(static_cast<double>(v.x),static_cast<double>(v.y));
    if (n==0.0) throw std::domain_error("normalize: zero vector");
    return {static_cast<float>(v.x/n),static_cast<float>(v.y/n)};
}
inline Vec3 normalize(Vec3 v) {
    if (!is_finite(v)) throw std::domain_error("normalize: non-finite vector");
    const double n=std::hypot(static_cast<double>(v.x),static_cast<double>(v.y),static_cast<double>(v.z));
    if (n==0.0) throw std::domain_error("normalize: zero vector");
    return {static_cast<float>(v.x/n),static_cast<float>(v.y/n),static_cast<float>(v.z/n)};
}
inline Vec4 normalize(Vec4 v) {
    if (!is_finite(v)) throw std::domain_error("normalize: non-finite vector");
    const double n=std::hypot(std::hypot(static_cast<double>(v.x),static_cast<double>(v.y)),
                              std::hypot(static_cast<double>(v.z),static_cast<double>(v.w)));
    if (n==0.0) throw std::domain_error("normalize: zero vector");
    return {static_cast<float>(v.x/n),static_cast<float>(v.y/n),
            static_cast<float>(v.z/n),static_cast<float>(v.w/n)};
}

struct Mat4 {
    std::array<float,16> data{}; // Row-major storage; vectors multiply on the right.
    static Mat4 identity() {
        Mat4 m;
        m(0,0)=m(1,1)=m(2,2)=m(3,3)=1.0f;
        return m;
    }
    float& operator()(std::size_t row,std::size_t col) { return data[row*4+col]; }
    float operator()(std::size_t row,std::size_t col) const { return data[row*4+col]; }
};

inline Mat4 operator*(const Mat4& a,const Mat4& b) {
    Mat4 result;
    for (std::size_t r=0;r<4;++r) {
        for (std::size_t c=0;c<4;++c) {
            double value=0.0;
            for (std::size_t k=0;k<4;++k) value+=static_cast<double>(a(r,k))*b(k,c);
            result(r,c)=static_cast<float>(value);
        }
    }
    return result;
}

inline Vec4 operator*(const Mat4& m,Vec4 v) {
    const std::array<float,4> a{v.x,v.y,v.z,v.w};
    std::array<float,4> result{};
    for (std::size_t r=0;r<4;++r) {
        double value=0.0;
        for (std::size_t c=0;c<4;++c) value+=static_cast<double>(m(r,c))*a[c];
        result[r]=static_cast<float>(value);
    }
    return {result[0],result[1],result[2],result[3]};
}

inline Mat4 translation(Vec3 t) {
    if (!is_finite(t)) throw std::domain_error("translation: non-finite displacement");
    Mat4 m=Mat4::identity();
    m(0,3)=t.x; m(1,3)=t.y; m(2,3)=t.z;
    return m;
}
inline Mat4 scaling(Vec3 s) {
    if (!is_finite(s)) throw std::domain_error("scaling: non-finite scale");
    Mat4 m=Mat4::identity();
    m(0,0)=s.x; m(1,1)=s.y; m(2,2)=s.z;
    return m;
}
inline Mat4 rotation_y(float radians) {
    if (!std::isfinite(radians)) throw std::domain_error("rotation_y: non-finite angle");
    Mat4 m=Mat4::identity();
    const float c=std::cos(radians),s=std::sin(radians);
    m(0,0)=c; m(0,2)=s; m(2,0)=-s; m(2,2)=c;
    return m;
}
inline Mat4 perspective(float fovy_radians,float aspect,float near_plane,float far_plane) {
    constexpr double pi=3.1415926535897932384626433832795;
    if (!std::isfinite(fovy_radians)||!std::isfinite(aspect)||
        !std::isfinite(near_plane)||!std::isfinite(far_plane)||
        !(fovy_radians>0.0f&&static_cast<double>(fovy_radians)<pi)||
        !(aspect>0.0f)||!(near_plane>0.0f)||!(far_plane>near_plane)) {
        throw std::domain_error("perspective: invalid frustum");
    }
    const double f=1.0/std::tan(static_cast<double>(fovy_radians)*0.5);
    const double n=near_plane,z=far_plane;
    Mat4 m;
    m(0,0)=static_cast<float>(f/aspect); m(1,1)=static_cast<float>(f);
    m(2,2)=static_cast<float>((z+n)/(n-z));
    m(2,3)=static_cast<float>(2.0*z*n/(n-z)); m(3,2)=-1.0f;
    for (float value:m.data) {
        if (!std::isfinite(value)) throw std::domain_error("perspective: unrepresentable matrix");
    }
    return m;
}
inline Mat4 look_at(Vec3 eye,Vec3 target,Vec3 up) {
    if (!is_finite(eye)||!is_finite(target)||!is_finite(up)) {
        throw std::domain_error("look_at: non-finite camera");
    }
    const Vec3 forward=normalize(target-eye);
    const Vec3 right=normalize(cross(forward,normalize(up)));
    const Vec3 camera_up=cross(right,forward);
    Mat4 m=Mat4::identity();
    m(0,0)=right.x; m(0,1)=right.y; m(0,2)=right.z; m(0,3)=-dot(right,eye);
    m(1,0)=camera_up.x; m(1,1)=camera_up.y; m(1,2)=camera_up.z; m(1,3)=-dot(camera_up,eye);
    m(2,0)=-forward.x; m(2,1)=-forward.y; m(2,2)=-forward.z; m(2,3)=dot(forward,eye);
    for (float value:m.data) {
        if (!std::isfinite(value)) throw std::domain_error("look_at: unrepresentable matrix");
    }
    return m;
}

} // namespace mr
