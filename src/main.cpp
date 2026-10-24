#include "miniraster/rasterizer.hpp"
#include "miniraster/scenes.hpp"
#include "miniraster/cli_utf8.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
int number(const std::string& text,int minimum,int maximum) {
    std::size_t consumed=0; const int result=std::stoi(text,&consumed);
    if(consumed!=text.size()||result<minimum||result>maximum) throw std::invalid_argument("integer option outside allowed range");
    return result;
}
void help() {
    std::cout << "Usage: miniraster [options]\n"
        "  --scene triangle|cube|perspective|nearclip|small|overdraw\n"
        "  --mode reference|tile|parallel --tile 16 --threads 4\n"
        "  --width 960 --height 640 --complexity 4096 --angle 0.0\n"
        "  --shader color|texture|phong --filter nearest|bilinear --affine\n"
        "  --output output/demo.ppm --help --version\n"
        "All geometry and textures are generated locally; no GPU rendering is used.\n";
}
}
int main(int argc,char* argv[]) {
    try {
        const auto arguments=mr::utf8_arguments(argc,argv);
        argc=static_cast<int>(arguments.size());
        int width=960,height=640,complexity=4096;
        int threads=static_cast<int>(std::min(8u,std::max(1u,std::thread::hardware_concurrency())));
        float angle=0;
        std::string scene_name="cube",output="output/demo.ppm";
        mr::RenderOptions options; options.mode=mr::RenderMode::TileParallel;
        options.shader=mr::ShaderMode::BlinnPhong;
        for(int i=1;i<argc;++i) {
            const std::string key=arguments[static_cast<std::size_t>(i)];
            if(key=="--help") { help(); return 0; }
            if(key=="--version") { std::cout<<"MiniRaster "<<MINIRASTER_VERSION<<" (CPU rasterizer)\n"; return 0; }
            if(key=="--affine") { options.perspective_correct=false; continue; }
            if(i+1>=argc) throw std::invalid_argument("missing option value: "+key);
            const std::string value=arguments[static_cast<std::size_t>(++i)];
            if(key=="--scene") scene_name=value;
            else if(key=="--output") output=value;
            else if(key=="--width") width=number(value,1,4096);
            else if(key=="--height") height=number(value,1,4096);
            else if(key=="--complexity") complexity=number(value,1,200000);
            else if(key=="--tile") options.tile_size=number(value,1,512);
            else if(key=="--threads") threads=number(value,1,64);
            else if(key=="--angle") {
                std::size_t used=0; angle=std::stof(value,&used);
                if(used!=value.size()||!std::isfinite(angle)) throw std::invalid_argument("angle must be finite radians");
            } else if(key=="--mode") {
                if(value=="reference") options.mode=mr::RenderMode::Reference;
                else if(value=="tile") options.mode=mr::RenderMode::TileSerial;
                else if(value=="parallel") options.mode=mr::RenderMode::TileParallel;
                else throw std::invalid_argument("unknown mode");
            } else if(key=="--shader") {
                if(value=="color") options.shader=mr::ShaderMode::VertexColor;
                else if(value=="texture") options.shader=mr::ShaderMode::Textured;
                else if(value=="phong") options.shader=mr::ShaderMode::BlinnPhong;
                else throw std::invalid_argument("unknown shader");
            } else if(key=="--filter") {
                if(value=="nearest") options.filter=mr::Filter::Nearest;
                else if(value=="bilinear") options.filter=mr::Filter::Bilinear;
                else throw std::invalid_argument("unknown filter");
            } else throw std::invalid_argument("unknown option: "+key);
        }
        const auto scene=mr::make_scene(scene_name,static_cast<float>(width)/height,complexity,7,angle);
        options.texture=&scene.texture; options.camera_position=scene.camera_position;
        mr::Framebuffer target(width,height); mr::Rasterizer renderer;
        std::unique_ptr<mr::ThreadPool> pool;
        if(options.mode==mr::RenderMode::TileParallel) pool=std::make_unique<mr::ThreadPool>(static_cast<std::size_t>(threads));
        const auto start=std::chrono::steady_clock::now();
        target.clear({0.025f,0.035f,0.055f});
        const auto geometry=mr::prepare_scene(scene);
        const auto stats=renderer.render(geometry.triangles,target,options,pool.get());
        const double frame_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        const auto path=std::filesystem::u8path(output);
        if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        target.write_ppm(output);
        std::cout<<std::fixed<<std::setprecision(3)<<"Rendered "<<scene_name<<" -> "<<output<<'\n'
            <<"source_triangles="<<geometry.source_triangles<<" clipped_triangles="<<geometry.triangles.size()
            <<" shaded_fragments="<<stats.shaded_fragments<<'\n'
            <<"frame_ms="<<frame_ms<<" transform_clip_ms="<<geometry.transform_clip_ms
            <<" prepare_ms="<<stats.prepare_ms<<" bin_ms="<<stats.bin_ms<<" raster_ms="<<stats.raster_ms<<'\n'
            <<"Timing excludes image encoding, file output, and pool construction. Use render_bench for comparisons.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"error: "<<error.what()<<'\n'; return 2;
    }
}
