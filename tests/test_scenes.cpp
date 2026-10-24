#include "miniraster/rasterizer.hpp"
#include "miniraster/scenes.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        std::size_t checks=0;
        const auto require=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
        mr::Rasterizer renderer; mr::ThreadPool pool(3);
        for(const std::string name:{"triangle","cube","perspective","nearclip","small","overdraw"}) {
            const auto scene=mr::make_scene(name,97.0f/61.0f,100);
            const auto prepared=mr::prepare_scene(scene);
            require(!prepared.triangles.empty(),"scene has no visible geometry");
            mr::RenderOptions opts; opts.texture=&scene.texture;opts.shader=mr::ShaderMode::Textured;
            mr::Framebuffer reference(97,61); reference.clear({0.02f,0.03f,0.05f});
            renderer.render(prepared.triangles,reference,opts);
            std::size_t visible=0;
            for(const auto depth:reference.depths()) if(std::isfinite(depth)) ++visible;
            require(visible>0,"scene generated no visible samples");
            for(const int tile:{7,16,32}) for(const auto mode:{mr::RenderMode::TileSerial,mr::RenderMode::TileParallel}) {
                mr::Framebuffer target(97,61); target.clear({0.02f,0.03f,0.05f});
                opts.mode=mode;opts.tile_size=tile;
                renderer.render(prepared.triangles,target,opts,&pool);
                for(std::size_t i=0;i<target.colors().size();++i) {
                    const auto a=target.colors()[i],b=reference.colors()[i];
                    require(a.x==b.x&&a.y==b.y&&a.z==b.z,"scene colors differ between traversal modes");
                    require(target.depths()[i]==reference.depths()[i],"scene depth differs between traversal modes");
                }
            }
        }
        for(const bool near_plane:{false,true}) {
            const float left=near_plane?-0.8f:-2.0f;
            const float zleft=near_plane?-2.0f:0.0f;
            mr::ClipVertex a{{left,-0.8f,zleft,1},{0,1},{0,0,1},{1,1,1},{}};
            mr::ClipVertex b{{0.8f,-0.8f,0,1},{1,1},{0,0,1},{1,1,1},{}};
            mr::ClipVertex c{{0.8f,0.8f,0,1},{1,0},{0,0,1},{1,1,1},{}};
            mr::ClipVertex d{{left,0.8f,zleft,1},{0,0},{0,0,1},{1,1,1},{}};
            std::vector<mr::ClipTriangle> pieces;
            for(const auto& triangle:{mr::ClipTriangle{a,b,c},mr::ClipTriangle{a,c,d}}) {
                const auto clipped=mr::clip_triangle(triangle);
                pieces.insert(pieces.end(),clipped.begin(),clipped.end());
            }
            mr::Framebuffer target(30,30);target.clear();
            std::vector<std::uint32_t> coverage(900,0);
            mr::RenderOptions options;options.coverage=&coverage;
            renderer.render(pieces,target,options);
            for(int y=0;y<30;++y)for(int x=0;x<30;++x) {
                const bool expected=x>=(near_plane?15:0)&&x<27&&y>=3&&y<27;
                require(coverage[static_cast<std::size_t>(y)*30+x]==(expected?1u:0u),
                        "clipped adjacent triangles have gaps or duplicate coverage");
            }
        }
        bool rejected=false;try{mr::make_scene("missing",1);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"unknown scene was accepted");
        std::cout<<"scene integration: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
