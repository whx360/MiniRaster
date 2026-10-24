#include "miniraster/rasterizer.hpp"
#include "miniraster/scenes.hpp"
#include "miniraster/cli_utf8.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using Clock=std::chrono::steady_clock;
struct Case { mr::RenderMode mode; int tile,threads; const char* name; };
bool same(const mr::Framebuffer& a,const mr::Framebuffer& b) {
    if(a.width()!=b.width()||a.height()!=b.height()) return false;
    for(std::size_t i=0;i<a.colors().size();++i) {
        const auto x=a.colors()[i],y=b.colors()[i];
        if(x.x!=y.x||x.y!=y.y||x.z!=y.z||a.depths()[i]!=b.depths()[i]) return false;
    }
    return true;
}
int integer(const std::string& text,int lo,int hi) {
    std::size_t n=0;const int x=std::stoi(text,&n);
    if(n!=text.size()||x<lo||x>hi) throw std::invalid_argument("benchmark option out of range");
    return x;
}
}
int main(int argc,char* argv[]) {
    try {
        const auto arguments=mr::utf8_arguments(argc,argv);
        argc=static_cast<int>(arguments.size());
        int width=640,height=360,repeats=7,warmups=2,max_threads=8,order_seed=20261023;
        std::string output="benchmarks/results/raw.csv";
        for(int i=1;i<argc;++i) {
            const std::string key=arguments[static_cast<std::size_t>(i)];
            if(key=="--help") { std::cout<<"render_bench --output raw.csv --width 640 --height 360 --repeats 7 --warmups 2 --max-threads 8 --order-seed 20261023\n";return 0; }
            if(i+1>=argc) throw std::invalid_argument("missing benchmark option value");
            const std::string value=arguments[static_cast<std::size_t>(++i)];
            if(key=="--output") output=value;
            else if(key=="--width") width=integer(value,16,2048);
            else if(key=="--height") height=integer(value,16,2048);
            else if(key=="--repeats") repeats=integer(value,3,100);
            else if(key=="--warmups") warmups=integer(value,1,20);
            else if(key=="--max-threads") max_threads=integer(value,1,32);
            else if(key=="--order-seed") order_seed=integer(value,0,2147483647);
            else throw std::invalid_argument("unknown benchmark option");
        }
        std::vector<Case> cases{{mr::RenderMode::Reference,16,1,"reference"}};
        for(int tile:{8,16,32,64}) cases.push_back({mr::RenderMode::TileSerial,tile,1,"tile"});
        for(int tile:{8,16,32,64}) for(int threads:{1,2,4,8})
            if(threads<=max_threads) cases.push_back({mr::RenderMode::TileParallel,tile,threads,"parallel"});
        std::mt19937 order_rng(static_cast<std::mt19937::result_type>(order_seed));
        std::shuffle(cases.begin(),cases.end(),order_rng);
        const auto path=std::filesystem::u8path(output);
        if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        std::ofstream csv(path); if(!csv) throw std::runtime_error("cannot write benchmark CSV");
        csv<<"scene,width,height,mode,tile,threads,sample,frame_ms,geometry_ms,prepare_ms,bin_ms,raster_ms,source_triangles,clipped_triangles,tile_references,auxiliary_bytes,framebuffer_bytes,shaded_fragments,exact_match\n"<<std::fixed<<std::setprecision(6);
        mr::Rasterizer renderer;
        for(const std::string scene_name:{"cube","small","overdraw"}) {
            const int complexity=scene_name=="overdraw"?48:4096;
            const auto scene=mr::make_scene(scene_name,static_cast<float>(width)/height,complexity);
            const auto prepared=mr::prepare_scene(scene);
            mr::Framebuffer reference(width,height); reference.clear({0.025f,0.035f,0.055f});
            mr::RenderOptions opts; opts.texture=&scene.texture; opts.shader=mr::ShaderMode::Textured;
            opts.camera_position=scene.camera_position;
            renderer.render(prepared.triangles,reference,opts);
            for(const auto& setting:cases) {
                mr::Framebuffer target(width,height);
                opts.mode=setting.mode; opts.tile_size=setting.tile;
                std::unique_ptr<mr::ThreadPool> pool;
                if(setting.mode==mr::RenderMode::TileParallel) pool=std::make_unique<mr::ThreadPool>(setting.threads);
                target.clear({0.025f,0.035f,0.055f}); renderer.render(prepared.triangles,target,opts,pool.get());
                if(!same(reference,target)) throw std::runtime_error("image/depth mismatch before timing: "+scene_name);
                double total=0;
                for(int sample=-warmups;sample<repeats;++sample) {
                    const auto start=Clock::now();
                    target.clear({0.025f,0.035f,0.055f});
                    mr::RenderStats stats;
                    double geometry_ms=0;
                    {
                        const auto current=mr::prepare_scene(scene);
                        geometry_ms=current.transform_clip_ms;
                        stats=renderer.render(current.triangles,target,opts,pool.get());
                    }
                    const double frame_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
                    if(sample<0) continue;
                    if(!same(reference,target)) throw std::runtime_error("timed frame/depth mismatch: "+scene_name);
                    total+=frame_ms;
                    csv<<scene_name<<','<<width<<','<<height<<','<<setting.name<<','<<setting.tile<<','<<setting.threads<<','<<sample<<','
                       <<frame_ms<<','<<geometry_ms<<','<<stats.prepare_ms<<','<<stats.bin_ms<<','<<stats.raster_ms<<','
                       <<prepared.source_triangles<<','<<prepared.triangles.size()<<','<<stats.tile_references<<','<<stats.auxiliary_bytes<<','
                       <<target.colors().size()*(sizeof(mr::Vec3)+sizeof(float))<<','<<stats.shaded_fragments<<",1\n";
                }
                csv.flush();
                if(!csv) throw std::runtime_error("benchmark CSV write failed");
                std::cout<<scene_name<<' '<<setting.name<<" tile="<<setting.tile<<" threads="<<setting.threads
                         <<" mean_ms="<<std::fixed<<std::setprecision(3)<<total/repeats<<" exact_match=1\n";
            }
        }
        std::cout<<"Saved "<<output<<". Equality validation and file output are outside timed frames.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"benchmark error: "<<error.what()<<'\n';return 2; }
}
