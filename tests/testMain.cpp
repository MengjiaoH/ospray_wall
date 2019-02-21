#include <iostream>
#include <thread>
#include <mutex>
#include <random>
//#include "common.h"
// ospray side
#include "ospcommon/vec.h"
#include "ospcommon/networking/Socket.h"
#include "ospcommon/networking/SocketFabric.h"
#include "ospcommon/networking/BufferedDataStreaming.h"
#include "mpiCommon/MPICommon.h"
#include "ospcommon/tasking/parallel_for.h"

#include "../render_clients/Client.h"
#include "mpi.h"
#include "../common/ServiceInfo.h"
// OSPRay
#include "ospray/ospray_cpp/Device.h"
#include "ospray/ospray_cpp/Data.h"
#include "ospray/ospray_cpp/Camera.h"
#include "ospray/ospray_cpp/FrameBuffer.h"
#include "ospray/ospray_cpp/Renderer.h"
#include "ospray/ospray_cpp/Geometry.h"
#include "ospray/ospray_cpp/Model.h"


std::mutex sendTileMutex;

namespace ospray{
    namespace dw{
        using namespace ospcommon;
        using namespace ospray::cpp;
        std::random_device device;
        std::mt19937 gen(device());
        std::uniform_int_distribution<std::mt19937::result_type> dist(1, 255);


void renderFrame(const mpicommon::Group &me, Client *client)
    {
      static size_t frameID = 0;
      static double lastTime = getSysTime();

      assert(client);
      const vec2i totalPixels = client->totalPixelsInWall();
      //std::cout << "check total pixels " << totalPixels.x << " " << totalPixels.y << std::endl;
      vec2i tileSize(256);
      vec2i numTiles = divRoundUp(totalPixels,tileSize);
      size_t tileCount = numTiles.product();
       //std::cout << "tileCount = " << tileCount << std::endl;
      tasking::parallel_for((int)tileCount,[&](int tileID){
          if ((tileID % me.size) != me.rank)
            return;

          PlainTile tile(tileSize);
          const int tile_x = tileID % numTiles.x;
          const int tile_y = tileID / numTiles.x;
          
          tile.region.lower = vec2i(tile_x,tile_y)*tileSize;
          tile.region.upper = min(tile.region.lower+tileSize,totalPixels);
          int rgba, r, g, b;
          box2i quad;
          quad.lower.x = totalPixels.x / 2; quad.upper.x = totalPixels.x;
          quad.lower.y = totalPixels.y / 2; quad.upper.y = totalPixels.y;              
          
        //   int r = dist(gen);
        //   int g = dist(gen);
        //   int b = dist(gen);
        //   rgba = (b << 16) + (g << 8) + (r << 0);

          for (int iy=tile.region.lower.y;iy<tile.region.upper.y;iy++)
            for (int ix=tile.region.lower.x;ix<tile.region.upper.x;ix++) {
#if 0
              int r = (frameID+ix) % 255;
              int g = (frameID+iy) % 255;
              int b = (frameID+ix+iy) % 255;
              int rgba = (b<<16)+(g<<8)+(r<<0);
#else
              //int r = dist(gen);
              //int g = dist(gen);
              //int b = dist(gen);
              //rgba = (b << 16) + (g << 8) + (r << 0);
            //   if( ix >= 0 && ix <= 256 && iy >=0 && iy <= 256){
            //     int r = 255.0;
            //     int g = 0.0;
            //     int b = 0.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }else if(ix > 256 && ix <= 512 && iy >=0 && iy <= 256){
            //     int r = 0.0;
            //     int g = 255.0;
            //     int b = 0.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }else if(ix >= 0 && ix <= 256 && iy > 256 && iy <= 512){
            //     int r = 0.0;
            //     int g = 0.0;
            //     int b = 255.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }else if(ix > 256 && ix <= 512 && iy >256 && iy <= 512){
            //     int r = 255.0;
            //     int g = 255.0;
            //     int b = 0.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   } 
            //   else if(ix > 512 && ix <= 768 && iy > 0 && iy <= 256){
            //     int r = 241.0;
            //     int g = 182.0;
            //     int b = 218.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   } else if(ix > 768 && ix <= 1024 && iy >= 0 && iy <= 256){
            //     int r = 146.0;
            //     int g = 197.0;
            //     int b = 222.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }else if(ix >= 512 && ix <= 768 && iy > 256 && iy <= 512){
            //     int r = 169.0;
            //     int g = 219.0;
            //     int b = 160.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }else if(ix > 768 && ix <= 1024 && iy > 256 && iy <= 512){
            //     int r = 255.0;
            //     int g = 255.0;
            //     int b = 191.0;
            //     rgba = (b << 16) + (g << 8) + (r << 0);
            //   }
              r = (frameID+(ix>>2)) % 255;
              g = (frameID+(iy>>2)) % 255;
              b = (frameID+(ix>>2)+(iy>>2)) % 255;
              rgba = (b<<16)+(g<<8)+(r<<0);
#endif
              tile.pixel[(ix-tile.region.lower.x)+tileSize.x*(iy-tile.region.lower.y)] = rgba;
            }

          assert(client);
        //   std::cout << "tile ID " << tileID << std::endl;
        //   std::lock_guard<std::mutex> lock(sendTileMutex);
        //   std::string filename = "client_afterCopy" + std::to_string(me.rank) + "_"
        //                                                           + std::to_string(tile.region.lower.x) + "_"
        //                                                           + std::to_string(tile.region.lower.y) + "_"
        //                                                           + std::to_string(tile.region.upper.x) + "_"
        //                                                           + std::to_string(tile.region.upper.y) + ".ppm";
                        
                        
        //  ospcommon::vec2i img_size(tile.region.upper.x - tile.region.lower.x, tile.region.upper.y - tile.region.lower.y);
 
        //  writePPM(filename.c_str(), &img_size, (uint32_t*)tile.pixel);
         client->writeTile(tile);
        });
      ++frameID;
      double thisTime = getSysTime();
    //   printf("done rendering frame %li (%f fps)\n",frameID,1.f/(thisTime-lastTime));
      lastTime = thisTime;
      me.barrier();
      client->endFrame();
    }

extern "C" int main(int argc, const char **argv){
    if(ospInit(&argc, argv) != OSP_NO_ERROR){
        throw std::runtime_error("Cannot Initialize OSPRay");
    }else{
        std::cout << "Initializa OSPRay Successfully" << std::endl;
    }

    std::string hostName;
	int portNum = 2903;
    int remote_mode = 0;
	for(int i = 1; i < argc; i++){
		std::string str(argv[i]);
		if(str == "-port"){
			portNum = std::atoi(argv[++i]);
		}else if(str == "-hostname"){
            hostName = argv[++i];
        }else if(str == "-remote"){
            remote_mode = std::atoi(argv[++i]);
        }
	}
    mpicommon::init(&argc, argv, true);
    mpicommon::Group world(MPI_COMM_WORLD);
   
    mpicommon::Group me = world.dup();

    Client *client;

    int infoPortNum = 8443;
    ServiceInfo serviceInfo;
    WallInfo wallInfo;
    char *data = new char[sizeof(WallInfo)];
    if(me.rank == 0){
        serviceInfo.getFrom(hostName, infoPortNum);
        wallInfo = *serviceInfo.wallInfo;
        // std::cout << "=============== Wall Config Info ==============" << std::endl;
        // std::cout << "service info mpiPortName " << serviceInfo.mpiPortName << std::endl;
        // std::cout << serviceInfo.wallInfo ->totalPixelsInWall.x << " " << serviceInfo.wallInfo ->totalPixelsInWall.y << std::endl;
    }
    // me.barrier();
    MPI_CALL(Bcast(&wallInfo, sizeof(WallInfo), MPI_BYTE, 0, me.comm));

    // mpicommon::world.barrier();
    //  std::cout << "rank " << me.rank << " size = " <<  wallInfo.numDisplays.x << " " <<
    //                                                                               wallInfo.numDisplays.y<< std::endl;
    // Send wallinfo to other ranks 
    client = new Client(hostName, portNum, me,  &wallInfo);


    int frames = 0;
    while(1){
        frames++;
        double lastTime = getSysTime();
        renderFrame(me, client);
        double thisTime = getSysTime();
        // printf("done rendering frame %li (%f fps)\n",frames, 1.f/(thisTime-lastTime));
    }
    
       
    //std::cout << "frame rate = " << (float)frames / (thisTime - lastTime) << std::endl;
    MPI_Finalize();
    return 0;
}
}
}
