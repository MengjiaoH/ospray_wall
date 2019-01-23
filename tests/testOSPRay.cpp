/*
    Test OSPRay Renerer with displaywall
   */

// std
#include <vector>
#include <random>
#include <chrono>

#include "mpi.h"
#include "mpiCommon/MPICommon.h"
#include "../render_clients/Client.h"
#include "testOSPRay.h"
//helper
#include "../common/helper.h"
#include "../common/ServiceInfo.h"
#include "geometry.h"

// OSPRay
#include "ospcommon/vec.h"
#include "ospcommon/box.h"
#include "ospray/ospray_cpp/Device.h"
#include "ospray/ospray_cpp/Data.h"
#include "ospray/ospray_cpp/Camera.h"
#include "ospray/ospray_cpp/FrameBuffer.h"
#include "ospray/ospray_cpp/Renderer.h"
#include "ospray/ospray_cpp/Geometry.h"
#include "ospray/ospray_cpp/Model.h"



namespace ospray{
    namespace dw{
        using namespace ospcommon;
        using namespace ospray::cpp;
        
        extern "C" int main(int ac, const char **av){
          
            // parse arguments 
            // hard code parameters by now
            std::vector<std::string> inputfiles;
            std::string filename;
            std::string hostName;
            int portNum;
            int remote_mode = 0;
            int mode = 0;
	    std::string bobj_filename;
            for(int i = 1; i < ac; ++i)
            {
                std::string str(av[i]);
                if(str == "-hostname"){
                    hostName = av[++i];
                }else if(str == "-file"){
                    filename = av[++i];
                }
                else if(str == "-port"){
                    portNum = std::atoi(av[++i]);
                }
                // else if(str == "-remote"){
                //     remote_mode = std::atoi(av[++i]);
                // }
                else if(str == "-mode"){
                    mode = std::atoi(av[++i]);
                }
		else if (str == "-bobj") {
			mode = 2;
			bobj_filename = av[++i];
		}
                //}else{
                    //filename = av[++i];
                    ////inputfiles.push_back(av[i]);
                //}
            }
            // Load Modules
            ospLoadModule("wall");   
            ospLoadModule("tubes");

            if(ospInit(&ac, av) != OSP_NO_ERROR){
                throw std::runtime_error("Cannot Initialize OSPRay");
            }else{
                std::cout << "Initializa OSPRay Successfully" << std::endl;
            }

            vec2i canvas;
            int stereo;
            std::string mpiPortName; 

            ServiceInfo serviceInfo;
            WallInfo wallInfo;
            int infoPortNum = 8443;
            serviceInfo.getFrom(hostName,infoPortNum);
            wallInfo = *serviceInfo.wallInfo;
            mpiPortName = serviceInfo.mpiPortName;
            stereo = wallInfo.stereo;
            canvas = wallInfo.totalPixelsInWall;

              // triangle mesh data
              float vertex[] = { -1.0f, -1.0f, 3.0f, 0.f,
                                 -1.0f,  1.0f, 3.0f, 0.f,
                                  1.0f, -1.0f, 3.0f, 0.f,
                                  0.1f,  0.1f, 0.3f, 0.f };
              float color[] =  { 0.9f, 0.5f, 0.5f, 1.0f,
                                 0.8f, 0.8f, 0.8f, 1.0f,
                                 0.8f, 0.8f, 0.8f, 1.0f,
                                 0.5f, 0.9f, 0.5f, 1.0f };
              int32_t index[] = { 0, 1, 2,
                                  1, 2, 3 };

            float box = 5000;
            //float cam_pos[] = {box, box, 0}
	    float cam_pos[] = {100, 62, 200};
            //float cam_pos[] = {1023, -100, -200};
            float cam_up[] = {0, 1, 0};
	    float cam_target[] = {152, 62, 62};
            //float cam_target[] = {1023, 1023, 975};
            float cam_view[] = {cam_target[0]-cam_pos[0], cam_target[1] - cam_pos[1], cam_target[2] - cam_pos[2]};
            OSPGeometry geom;
            //canvas.x = 1024; canvas.y = 768;
            if (mode == 0){
                // spheres
                // construct particles
                std::vector<Particle> random_atoms;
                std::vector<float> random_colors;
                constructParticles(random_atoms, random_colors, box);
                
                //new spheres geometry
                geom = commitParticles(random_atoms, random_colors);  // create and setup model and mesh
	    } else if (mode == 2) {
		    osp::vec3f coatcolor{0.17647058823529413, 0.3176470588235294, 0.6392156862745098};
    		    osp::vec3f matcolor{0.20392156862, 0.76078431372, 0.86666666666};
		    OSPMaterial isosurfaceMat = ospNewMaterial2("pathtracer", "CarPaint");
		    ospSetVec3f(isosurfaceMat, "baseColor", matcolor);
		    ospSetVec3f(isosurfaceMat, "coatColor", coatcolor);
		    ospSet1f(isosurfaceMat, "flakeDensity", 0.7f);
		    ospSet1f(isosurfaceMat, "roughness", 0.03f);
		    ospSet1f(isosurfaceMat, "coatRoughness", 0.2f);
		    ospSet1f(isosurfaceMat, "flipflopFalloff", 0.3f);
		    ospCommit(isosurfaceMat);

		    std::vector<vec3i> indexBuf;
		    std::vector<float> verts;
		    std::ifstream fin(bobj_filename.c_str(), std::ios::binary);
		    uint64_t header[2] = {0};
		    fin.read(reinterpret_cast<char*>(header), sizeof(header));
		    verts.resize(header[0] * 3, 0.f);
		    fin.read(reinterpret_cast<char*>(verts.data()), sizeof(float) * 3 * header[0]);
		    std::vector<uint64_t> indices(header[1] * 3, 0);
		    fin.read(reinterpret_cast<char*>(indices.data()), sizeof(uint64_t) * 3 * header[1]);
		    indexBuf.reserve(header[1]);
		    for (size_t i = 0; i < indices.size(); i += 3) {
			    indexBuf.push_back(vec3i(indices[i], indices[i + 1], indices[i + 2]));
		    }

		    std::cout << "Loaded mesh with " << verts.size() / 3 << " verts and "
			    << indexBuf.size() << " indices\n";

		    // We assume there's a single shape in each OBJ file, since that's
		    // what my mesh gridder and isosurface to obj tools output.
		    geom = ospNewGeometry("triangles");
		    OSPData vertsData = ospNewData(verts.size() / 3, OSP_FLOAT3, verts.data());
		    OSPData indexData = ospNewData(indexBuf.size(), OSP_INT3, indexBuf.data());
		    ospSetData(geom, "vertex", vertsData);
		    ospSetData(geom, "index", indexData);
		    ospSetMaterial(geom, isosurfaceMat);
		    ospSet1i(geom, "geom.materialID", 0);
		    ospCommit(geom);

            }else{
                // Lines
                std::cout << "start parsing data " << std::endl; 
                geom = tube(filename);
            }

            std::cout << "done parse data" << std::endl;     
 
            // create and setup model and mesh
            OSPModel model = ospNewModel();
            ospAddGeometry(model, geom);
            ospCommit(model);  

            OSPCamera camera = ospNewCamera("perspective");
            ospSet1f(camera, "aspect", canvas.x / (float)canvas.y);
            ospSet3fv(camera, "pos", cam_pos);
            ospSet3fv(camera, "up", cam_up);
            ospSet3fv(camera, "dir", cam_view);
            ospCommit(camera);

            // For distributed rendering we must use the MPI raycaster
            OSPRenderer renderer = ospNewRenderer("scivis");

            OSPLight ambient_light = ospNewLight(renderer, "AmbientLight");
            ospSet1f(ambient_light, "intensity", 0.25f);
            ospSetVec3f(ambient_light, "color", osp::vec3f{174.f / 255.0f, 218.0f / 255.f, 1.0f});
            ospCommit(ambient_light);
            OSPLight directional_light0 = ospNewLight(renderer, "DirectionalLight");
            ospSet1f(directional_light0, "intensity", 2.5f);
            //ospSetVec3f(directional_light0, "direction", osp::vec3f{80.f, 25.f, 35.f});
	    osp::vec3f lightdir{cam_view[0] + 0.25, cam_view[1] + 0.2, cam_view[2]};
            ospSetVec3f(directional_light0, "direction", lightdir);
            ospSetVec3f(directional_light0, "color", osp::vec3f{1.0f, 232.f / 255.0f, 166.0f / 255.f});
            ospCommit(directional_light0);
            OSPLight directional_light1 = ospNewLight(renderer, "DirectionalLight");
            ospSet1f(directional_light1, "intensity", 0.1f);
            ospSetVec3f(directional_light1, "direction", osp::vec3f{0.f, -1.f, 0.f});
            ospSetVec3f(directional_light1, "color", osp::vec3f{1.0f, 1.0f, 1.0f});
            ospCommit(directional_light1);
            std::vector<OSPLight> light_list {ambient_light, directional_light0, directional_light1} ;
            OSPData lights = ospNewData(light_list.size(), OSP_OBJECT, light_list.data());
            ospCommit(lights);


            //OSPLight light = ospNewLight(renderer, "ambient");
            //ospCommit(light);
            //OSPData lights = ospNewData(1, OSP_LIGHT, &light, 0);
            //ospCommit(lights);

            // Setup the parameters for the renderer
            ospSet1i(renderer, "spp", 1);
            //ospSet1i(renderer, "aoSamples", 4);
            //ospSet1i(renderer, "shadowsEnabled", 1);
            ospSet1f(renderer, "bgColor", 1.f);
            ospSetObject(renderer, "model", model);
            ospSetObject(renderer, "camera", camera);
            ospSetObject(renderer, "lights", lights);
            ospCommit(renderer);

            // Create framebuffer
            const osp::vec2i img_size{canvas.x, canvas.y};
            const osp::vec2i saved_img_size{canvas.x , canvas.y};
            OSPFrameBuffer pixelOP_framebuffer = ospNewFrameBuffer(img_size, OSP_FB_NONE,
			    OSP_FB_COLOR | OSP_FB_ACCUM);
            OSPFrameBuffer framebuffer = ospNewFrameBuffer(saved_img_size, OSP_FB_SRGBA, OSP_FB_COLOR);
            ospFrameBufferClear(framebuffer, OSP_FB_COLOR); 

            // Create pixelOP
            OSPPixelOp pixelOp = ospNewPixelOp("wall");
            // ospSet1i(pixelOp, "remote", remote_mode);
            ospSetString(pixelOp, "hostName", mpiPortName.c_str());
            ospSet1i(pixelOp, "portNum", portNum);
            OSPData wallInfoData = ospNewData(sizeof(WallInfo), OSP_RAW, &wallInfo, OSP_DATA_SHARED_BUFFER);
            ospCommit(wallInfoData);
            ospSetData(pixelOp, "wallInfo", wallInfoData);
            ospCommit(pixelOp);

            //Set pixelOp to the framebuffer
            ospSetPixelOp(pixelOP_framebuffer, pixelOp);

            int frameID = 0;
            using Time = std::chrono::duration<double, std::milli>;
            std::vector<Time> renderTime;
            using Stats = pico_bench::Statistics<Time>;

             //Render
            while(1){
                frameID++;
                 std::cout << "===================== Frame "  << frameID << " =================== " << "\n"
			 //<< std::flush;
               //auto lastTime = std::chrono::high_resolution_clock::now();
               ospRenderFrame(pixelOP_framebuffer, renderer, OSP_FB_COLOR);
               // ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR);
               //auto thisTime = std::chrono::high_resolution_clock::now();
               //renderTime.push_back(std::chrono::duration_cast<Time>(thisTime - lastTime));
                // std::cout << "Frame Rate  = " << 1.f / (thisTime - lastTime) << std::endl;

                 //double thisTime = getSysTime();
                 //std::cout << "offload frame rate = " << 1.f / (thisTime - lastTime) << std::endl;
                cam_pos[1] += 1.0;
                cam_view[1] = cam_target[1] - cam_pos[1];
                ospSet3fv(camera, "pos", cam_pos);
                ospSet3fv(camera, "dir", cam_view);
                ospCommit(camera);
            }

            if(!renderTime.empty()){
                Stats renderStats(renderTime);
                renderStats.time_suffix = "ms";
                std::cout  << "Decompression time statistics:\n" << renderStats << "\n";
            }



            // if(mpicommon::world.rank == 0){
            //     uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
            //     vec2i image_size; image_size.x = saved_img_size.x; image_size.y = saved_img_size.y;
            //     writePPM("test.ppm", &image_size, fb);
            //     std::cout << "Image saved to 'test.ppm'\n";
            //     ospUnmapFrameBuffer(fb, framebuffer);
            // }

            // Clean up all our objects
            //ospFreeFrameBuffer(framebuffer);
            ospFreeFrameBuffer(pixelOP_framebuffer);
            ospRelease(renderer);
            ospRelease(camera);
            ospRelease(model);
	        return 0;

        }
    }
}
