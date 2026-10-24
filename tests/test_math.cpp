#include "miniraster/math.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
int checks=0;
void check(bool condition,const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}
bool near(float a,float b,float tolerance=2.0e-5f) { return std::fabs(a-b)<=tolerance; }
void vector_near(mr::Vec4 a,mr::Vec4 b,const char* description) {
    check(near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z)&&near(a.w,b.w),description);
}
template<class Function> void rejects(Function function,const char* description) {
    bool rejected=false;
    try { function(); } catch (const std::domain_error&) { rejected=true; }
    check(rejected,description);
}
}

int main() {
    try {
        using namespace mr;
        static_assert(std::is_aggregate<Vec2>::value&&std::is_aggregate<Vec3>::value&&
                      std::is_aggregate<Vec4>::value,"vector API remains aggregate");
        constexpr float pi=3.14159265358979323846f;
        check(near(dot(Vec2{1,2},Vec2{3,4}),11),"2D dot hand calculation");
        check(near(cross(Vec2{1,2},Vec2{3,4}),-2),"2D signed cross hand calculation");
        const Vec2 a2=(Vec2{1,2}+Vec2{3,4}-Vec2{2,1})*2.0f/2.0f;
        check(near(a2.x,2)&&near(a2.y,5),"Vec2 arithmetic");
        const Vec3 a3=cross(Vec3{1,0,0},Vec3{0,1,0});
        check(near(a3.x,0)&&near(a3.y,0)&&near(a3.z,1),"right-handed cross");
        check(near(dot(Vec3{1,2,3},Vec3{4,5,6}),32),"Vec3 dot hand calculation");
        check(near(dot(Vec4{1,2,3,4},Vec4{5,6,7,8}),70),"Vec4 dot hand calculation");
        vector_near((Vec4{1,2,3,4}+Vec4{2,3,4,5}-Vec4{1,1,1,1})*2.0f/2.0f,
                    {2,4,6,8},"Vec4 arithmetic");
        check(near(length(Vec2{3,4}),5),"2D norm");
        check(near(length(Vec3{2,3,6}),7),"3D norm");
        check(near(length(Vec4{1,1,1,1}),2),"4D norm");
        const Vec3 unit=normalize(Vec3{3,0,4});
        check(near(unit.x,0.6f)&&near(unit.z,0.8f)&&near(length(unit),1),"normalize hand calculation");
        check(near(length(normalize(Vec2{2,3})),1),"Vec2 normalize");
        check(near(length(normalize(Vec4{2,3,4,5})),1),"Vec4 normalize");
        const float huge=std::numeric_limits<float>::max();
        check(near(length(normalize(Vec3{huge,huge,huge})),1),"normalize finite huge vectors");
        const float tiny=std::numeric_limits<float>::denorm_min();
        check(near(normalize(Vec3{tiny,0,0}).x,1),"normalize finite subnormal vectors");
        rejects([] { (void)normalize(Vec2{}); },"zero Vec2 rejected");
        rejects([] { (void)normalize(Vec3{}); },"zero Vec3 rejected");
        rejects([] { (void)normalize(Vec4{}); },"zero Vec4 rejected");
        rejects([] { (void)normalize(Vec3{std::numeric_limits<float>::infinity(),0,0}); },"infinity rejected");
        rejects([] { (void)normalize(Vec4{0,0,std::numeric_limits<float>::quiet_NaN(),0}); },"NaN rejected");

        const Vec4 point{1,2,3,1},direction{1,2,3,0};
        vector_near(Mat4::identity()*point,point,"identity");
        vector_near(translation({4,5,6})*point,{5,7,9,1},"point translation");
        vector_near(translation({4,5,6})*direction,direction,"translation preserves direction");
        vector_near(scaling({2,3,4})*point,{2,6,12,1},"scale");
        vector_near(rotation_y(pi/2)*Vec4{1,0,0,1},{0,0,-1,1},"right-handed Y rotation");
        vector_near((translation({4,5,6})*scaling({2,3,4}))*point,
                    {6,11,18,1},"rightmost transform first");
        const Mat4 model=translation({1,2,-3})*rotation_y(0.7f)*scaling({2,1,3});
        vector_near(model*point,translation({1,2,-3})*(rotation_y(0.7f)*(scaling({2,1,3})*point)),
                    "matrix composition matches sequential application");
        Mat4 indexing;
        indexing(2,3)=7;
        check(indexing.data[11]==7,"row-major indexing");

        const Mat4 projection=perspective(pi/2,2,1,11);
        vector_near(projection*Vec4{2,1,-1,1},{1,1,-1,1},"near-plane corner hand calculation");
        const Vec4 far=projection*Vec4{0,0,-11,1};
        check(near(far.z/far.w,1),"far maps to NDC +1");
        const Vec4 front=projection*Vec4{0,0,-3,1};
        check(front.w>0,"visible point has positive w");
        vector_near(look_at({0,0,3},{0,0,0},{0,1,0})*Vec4{0,0,3,1},{0,0,0,1},"eye maps to origin");
        vector_near(look_at({0,0,3},{0,0,0},{0,1,0})*Vec4{0,0,0,1},{0,0,-3,1},"target maps onto -Z");
        const Mat4 view=look_at({1,2,3},{-2,1,-1},{0,1,0});
        vector_near(view*Vec4{1,2,3,1},{0,0,0,1},"oblique eye maps to origin");
        Vec3 right{view(0,0),view(0,1),view(0,2)};
        Vec3 up{view(1,0),view(1,1),view(1,2)};
        Vec3 back{view(2,0),view(2,1),view(2,2)};
        check(near(length(right),1)&&near(length(up),1)&&near(length(back),1)&&
              near(dot(right,up),0)&&near(dot(up,back),0)&&near(dot(back,right),0),"camera basis orthonormal");
        check(near(dot(cross(right,up),back),1),"camera basis right-handed");
        rejects([] { (void)perspective(0,1,1,10); },"zero fovy rejected");
        rejects([pi] { (void)perspective(pi,1,1,10); },"pi fovy rejected");
        rejects([] { (void)perspective(1,0,1,10); },"zero aspect rejected");
        rejects([] { (void)perspective(1,1,0,10); },"zero near rejected");
        rejects([] { (void)perspective(1,1,2,1); },"reversed depth range rejected");
        rejects([] { (void)perspective(1,1,1,1); },"equal depth planes rejected");
        rejects([] { (void)perspective(1,1,1,std::numeric_limits<float>::infinity()); },"infinite far rejected");
        rejects([] { (void)look_at({0,0,0},{0,0,0},{0,1,0}); },"equal eye target rejected");
        rejects([] { (void)look_at({0,0,0},{0,0,-1},{0,0,1}); },"parallel up rejected");
        rejects([] { (void)look_at({0,0,0},{0,0,-1},{0,0,0}); },"zero up rejected");
        rejects([] { (void)translation({std::numeric_limits<float>::quiet_NaN(),0,0}); },"invalid translation rejected");
        rejects([] { (void)scaling({1,std::numeric_limits<float>::infinity(),1}); },"invalid scale rejected");
        rejects([] { (void)rotation_y(std::numeric_limits<float>::infinity()); },"invalid rotation rejected");
        std::cout<<"math: "<<checks<<" checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr<<"math failure after "<<checks<<" checks: "<<e.what()<<'\n';
        return EXIT_FAILURE;
    }
}
