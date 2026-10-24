#include "miniraster/clipper.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
int checks=0;
void check(bool condition,const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}
bool near(float a,float b,float tolerance=2.0e-6f) { return std::fabs(a-b)<=tolerance; }
mr::ClipVertex vertex(float x,float y,float z=0,float w=1) {
    return {{x,y,z,w},{x,y},{x,y,z},{x,y,z},{x,y,z}};
}
double area(const mr::ClipTriangle& t) {
    const double ax=static_cast<double>(t[0].clip.x)/t[0].clip.w;
    const double ay=static_cast<double>(t[0].clip.y)/t[0].clip.w;
    const double bx=static_cast<double>(t[1].clip.x)/t[1].clip.w;
    const double by=static_cast<double>(t[1].clip.y)/t[1].clip.w;
    const double cx=static_cast<double>(t[2].clip.x)/t[2].clip.w;
    const double cy=static_cast<double>(t[2].clip.y)/t[2].clip.w;
    return 0.5*((bx-ax)*(cy-ay)-(by-ay)*(cx-ax));
}
void inside(const std::vector<mr::ClipTriangle>& output) {
    for (const auto& t:output) {
        check(area(t)!=0.0,"output has nonzero projected area");
        for (const auto& v:t) {
            check(mr::is_finite(v.clip)&&mr::is_finite(v.uv)&&mr::is_finite(v.normal)&&
                  mr::is_finite(v.color)&&mr::is_finite(v.world),"all output components finite");
            check(v.clip.w>0&&v.clip.x>=-v.clip.w&&v.clip.x<=v.clip.w&&
                  v.clip.y>=-v.clip.w&&v.clip.y<=v.clip.w&&
                  v.clip.z>=-v.clip.w&&v.clip.z<=v.clip.w,"output satisfies all six planes");
        }
    }
}
template<class Function> void rejects(Function function,const char* description) {
    bool rejected=false;
    try { function(); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,description);
}
}

int main() {
    try {
        using namespace mr;
        const ClipTriangle basic{vertex(-0.5f,-0.5f),vertex(0.5f,-0.5f),vertex(0,0.5f)};
        const auto unchanged=clip_triangle(basic);
        check(unchanged.size()==1,"inside triangle unchanged");
        check(near(static_cast<float>(area(unchanged[0])),0.5f),"inside hand-calculated area");
        inside(unchanged);
        for (std::size_t i=0;i<3;++i) {
            check(unchanged[0][i].clip.x==basic[i].clip.x&&unchanged[0][i].clip.y==basic[i].clip.y&&
                  unchanged[0][i].uv.x==basic[i].uv.x,"inside vertices and attributes preserved");
        }

        // One vertex outside each plane produces a quad (two triangles).
        for (int plane=0;plane<6;++plane) {
            ClipTriangle t=basic;
            if (plane<2) t[2].clip.x=(plane==0?-2.0f:2.0f);
            else if (plane<4) t[2].clip.y=(plane==2?-2.0f:2.0f);
            else t[2].clip.z=(plane==4?-2.0f:2.0f);
            const auto clipped=clip_triangle(t);
            check(clipped.size()==2,"single-plane crossing creates quad");
            inside(clipped);
            ClipTriangle outside=basic;
            for (auto& v:outside) {
                if (plane<2) v.clip.x=(plane==0?-2.0f:2.0f);
                else if (plane<4) v.clip.y=(plane==2?-2.0f:2.0f);
                else v.clip.z=(plane==4?-2.0f:2.0f);
            }
            check(clip_triangle(outside).empty(),"fully outside plane rejected");
        }
        // The two top-plane intersections occur at t=1/2. Every attribute
        // must use that same edge parameter; normals are not normalized here.
        ClipTriangle attributes{vertex(-0.5f,0),vertex(0.5f,0),vertex(0,2)};
        attributes[0].uv={0,0}; attributes[1].uv={1,0}; attributes[2].uv={0.5f,1};
        attributes[0].normal={2,0,0}; attributes[1].normal={0,2,0}; attributes[2].normal={0,0,2};
        attributes[0].color={1,0,0}; attributes[1].color={0,1,0}; attributes[2].color={0,0,1};
        attributes[0].world={10,0,0}; attributes[1].world={0,10,0}; attributes[2].world={0,0,10};
        const auto quad=clip_triangle(attributes);
        int top_vertices=0;
        for (const auto& t:quad) for (const auto& v:t) {
            if (v.clip.y==v.clip.w) {
                ++top_vertices;
                check(near(v.uv.y,0.5f)&&near(v.normal.z,1)&&near(v.color.z,0.5f)&&
                      near(v.world.z,5),"all attributes linearly interpolated");
                if (v.clip.x<0) check(near(v.uv.x,0.25f)&&near(v.normal.x,1)&&near(v.world.x,5),"left intersection attributes");
                else check(near(v.uv.x,0.75f)&&near(v.normal.y,1)&&near(v.world.y,5),"right intersection attributes");
            }
        }
        check(top_vertices>=2,"both plane intersections observed");
        check(near(static_cast<float>(area(quad[0])+area(quad[1])),0.75f),"clipped trapezoid hand-calculated area");

        ClipTriangle varying_w{vertex(0,0,0,1),vertex(4,0,0,2),vertex(0,1,0,1)};
        varying_w[0].uv={0,0}; varying_w[1].uv={1,0}; varying_w[2].uv={0,1};
        const auto homogeneous=clip_triangle(varying_w);
        check(homogeneous.size()==2,"varying w crossing creates quad");
        inside(homogeneous);
        int right_intersections=0;
        for (const auto& t:homogeneous) for (const auto& v:t) {
            if (v.clip.x==v.clip.w) {
                ++right_intersections;
                check(near(v.clip.w,4.0f/3.0f)&&near(v.uv.x,1.0f/3.0f),
                      "homogeneous edge t differs from post-divide t");
            }
        }
        check(right_intersections>=2,"varying w right boundary found");

        ClipTriangle near_boundary=basic;
        for (auto& v:near_boundary) v.clip.z=-v.clip.w;
        check(clip_triangle(near_boundary).size()==1,"near plane boundary included");
        ClipTriangle far_boundary=basic;
        for (auto& v:far_boundary) v.clip.z=v.clip.w;
        check(clip_triangle(far_boundary).size()==1,"far plane boundary included");
        const ClipTriangle shared_boundary{vertex(-1,-1),vertex(1,-1),vertex(1,1)};
        check(clip_triangle(shared_boundary).size()==1,"clip-space corner triangle included");
        const ClipTriangle repeated{basic[0],basic[0],basic[2]};
        check(clip_triangle(repeated).empty(),"repeated vertices rejected");
        const ClipTriangle collinear{vertex(-0.5f,0),vertex(0,0),vertex(0.5f,0)};
        check(clip_triangle(collinear).empty(),"collinear triangle rejected");
        ClipTriangle reversed=basic;
        std::swap(reversed[1],reversed[2]);
        check(area(clip_triangle(reversed)[0])<0,"winding preserved");
        ClipTriangle tiny=basic;
        for (auto& v:tiny) v.clip=v.clip*1.0e-20f;
        check(clip_triangle(tiny).size()==1,"small positive homogeneous scale remains drawable");
        ClipTriangle behind=basic;
        for (auto& v:behind) v.clip.w=-1;
        check(clip_triangle(behind).empty(),"negative w triangle rejected");
        const ClipTriangle singular{vertex(0,0,0,0),basic[1],basic[2]};
        check(clip_triangle(singular).empty(),"projective zero vertex cannot create area");
        ClipTriangle across_eye{vertex(-0.8f,-0.5f,0,1),vertex(0.8f,-0.5f,0,1),vertex(0,0.8f,0,-0.5f)};
        inside(clip_triangle(across_eye));
        ClipTriangle invalid=basic;
        invalid[0].clip.x=std::numeric_limits<float>::quiet_NaN();
        rejects([&] { (void)clip_triangle(invalid); },"NaN position rejected");
        invalid=basic; invalid[0].uv.x=std::numeric_limits<float>::infinity();
        rejects([&] { (void)clip_triangle(invalid); },"infinite UV rejected");
        invalid=basic; invalid[0].world.z=std::numeric_limits<float>::quiet_NaN();
        rejects([&] { (void)clip_triangle(invalid); },"non-finite world attribute rejected");
        invalid=basic; invalid[0].normal.y=std::numeric_limits<float>::infinity();
        rejects([&] { (void)clip_triangle(invalid); },"non-finite normal rejected");
        invalid=basic; invalid[0].color.x=std::numeric_limits<float>::infinity();
        rejects([&] { (void)clip_triangle(invalid); },"non-finite color rejected");
        ClipTriangle large=basic;
        for (auto& v:large) v.clip=v.clip*(std::numeric_limits<float>::max()/2.0f);
        const auto large_output=clip_triangle(large);
        check(large_output.size()==1,"large finite homogeneous coordinates preserved");
        inside(large_output);

        // Seeded property checks: finite output, all planes, winding, idempotence.
        std::mt19937 random(20261023);
        std::uniform_real_distribution<float> coordinates(-4,4),positive_w(0.1f,3);
        for (int trial=0;trial<1200;++trial) {
            ClipTriangle t;
            for (auto& v:t) v=vertex(coordinates(random),coordinates(random),coordinates(random),positive_w(random));
            const auto output=clip_triangle(t);
            inside(output);
            for (const auto& part:output) {
                check(area(part)*area(t)>0,"clipping preserves random triangle winding");
                const auto again=clip_triangle(part);
                check(again.size()==1,"clipping output is idempotent");
                check(std::fabs(area(again[0])-area(part))<1.0e-12,"idempotence preserves area");
            }
        }
        std::cout<<"clipper: "<<checks<<" checks passed (1200 seeded property cases)\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr<<"clipper failure after "<<checks<<" checks: "<<e.what()<<'\n';
        return EXIT_FAILURE;
    }
}
