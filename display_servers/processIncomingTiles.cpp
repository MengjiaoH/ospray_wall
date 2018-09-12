
#include "Server.h"
#include "../common/CompressedTile.h"
#include "../common/helper.h"
#include "ospcommon/tasking/parallel_for.h"
#include <mutex>
#include <atomic>
#ifdef OSPRAY_TASKING_TBB
# include <tbb/task_scheduler_init.h>
#endif

namespace ospray {
  namespace dw {

#define DW_DBG(a) 

    using std::cout; 
    using std::endl;
    using std::flush;

    /*! the code that actually receives the tiles, decompresses
      them, and writes them into the current (write-)frame buffer */
    void Server::processIncomingTiles(mpicommon::Group &outside, const bool &use_tcp)
    {
      allocateFrameBuffers();
      //printf("tile receiver %i/%i: frame buffer(s) allocated; now receiving tiles\n",
              //displayGroup.rank,displayGroup.size);
      
      const box2i displayRegion = wallConfig.regionOfRank(displayGroup.rank);

#define THREADED_RECV 3
        
#if THREADED_RECV
      std::mutex displayMutex;
# ifdef OSPRAY_TASKING_TBB
      tbb::task_scheduler_init tbb_init;
# endif
      tasking::parallel_for(THREADED_RECV,[&](int) {

          void *decompressor = CompressedTile::createDecompressor();
          if(!use_tcp){
              while (1) {
                // -------------------------------------------------------
                // receive one tiles
                // -------------------------------------------------------

                CompressedTile encoded;
                encoded.receiveOne(outside);

                PlainTile plain(encoded.getRegion().size());
                encoded.decode(decompressor,plain);

                const box2i globalRegion = plain.region;
                size_t numWritten = 0;
                const uint32_t *tilePixel = plain.pixel;
                uint32_t *localPixel = plain.eye ? recv_r : recv_l;
                assert(localPixel);
                for (int iy=globalRegion.lower.y;iy<globalRegion.upper.y;iy++) {
                  
                  if (iy < displayRegion.lower.y) continue;
                  if (iy >= displayRegion.upper.y) continue;
                  
                  for (int ix=globalRegion.lower.x;ix<globalRegion.upper.x;ix++) {
                    if (ix < displayRegion.lower.x) continue;
                    if (ix >= displayRegion.upper.x) continue;
                    
                    const vec2i globalCoord(ix,iy);
                    const vec2i tileCoord = globalCoord-plain.region.lower;
                    const vec2i localCoord = globalCoord-displayRegion.lower;
                    const int localPitch = wallConfig.pixelsPerDisplay.x;
                    const int tilePitch  = plain.pitch;
                    const int tileOfs = tileCoord.x + tilePitch * tileCoord.y;
                    const int localOfs = localCoord.x + localPitch * localCoord.y;
                    localPixel[localOfs] = tilePixel[tileOfs];
                    ++numWritten;
                  }
                }

                {
#if THREADED_RECV
                  std::lock_guard<std::mutex> lock(displayMutex);
#endif
                  numWrittenThisFrame += numWritten;
                  // printf("written %li / %li\n",numWrittenThisFrame,numExpectedThisFrame);
                  if (numWrittenThisFrame == numExpectedThisFrame) {
                    DW_DBG(printf("display %i/%i has a full frame!\n",
                                  displayGroup.rank,displayGroup.size));
                    // displayGroup.barrier();
                    DW_DBG(printf("#osp:dw(%i/%i) barrier'ing on %i/%i\n",
                                  displayGroup.rank,displayGroup.size,
                                  outside.rank,outside.size));
                    //MPI_CALL(Barrier(outside.comm));
                    DW_DBG(printf("#osp:dw(%i/%i): DISPLAYING\n",
                                  displayGroup.rank,displayGroup.size));
              
                    displayCallback(recv_l,recv_r,objectForCallback);
                    // reset counter
                    numWrittenThisFrame = 0;
                    numExpectedThisFrame = wallConfig.displayPixelCount();
                    // and switch the in/out buffers
                    std::swap(recv_l,disp_l);
                    std::swap(recv_r,disp_r);
                  }
                }
              }
          }else{
              while (1) {
                // -------------------------------------------------------
                // receive one tiles
                // -------------------------------------------------------
                CompressedTile encoded;
                // std::cout << " ==========Processing incoming tiles ==========" << std::flush;
                MPI_Status status;
                MPI_CALL(Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, outside.comm, &status));
                MPI_CALL(Get_count(&status, MPI_BYTE, &encoded.numBytes));
                encoded.data = new unsigned char[encoded.numBytes];
                MPI_CALL(Recv(encoded.data, encoded.numBytes, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, outside.comm, &status));
                
                
                const CompressedTileHeader *header = (const CompressedTileHeader *)encoded.data;
                PlainTile plain(encoded.getRegion().size());
                encoded.decode(decompressor,plain);
                
                //static std::atomic<int> tileID;
                //int myTileID = tileID++;
                //std::cout << "tile ID = " << tileId << std::endl;

                //save compressed images 
                //std::cout << "DisplayGroup: write images after received " << std::endl;
                //std::string filename = "Display_receive"        + std::to_string(plain.region.lower.x) + "_"
                                                                //+ std::to_string(plain.region.lower.y) + "_"
                                                                 //+ std::to_string(plain.region.upper.x) + "_"
                                                                 //+ std::to_string(plain.region.upper.y) + ".ppm";
                //ospcommon::vec2i img_size(64, 64);
                //writePPM(filename.c_str(), &img_size, (uint32_t*)plain.pixel);
                
                // Decode Tiles Correctly!

                const box2i globalRegion = plain.region;
                //std::cout << "global region " << globalRegion.lower.x << " "
                                              //<< globalRegion.lower.y << " "
                                              //<< globalRegion.upper.x << " "
                                              //<< globalRegion.upper.y << std::endl;
                size_t numWritten = 0;
                const uint32_t *tilePixel = plain.pixel;

                uint32_t *localPixel = plain.eye ? recv_r : recv_l;
                assert(localPixel);
             
                //std::cout << "display region " << displayRegion.lower.x << " " << displayRegion.lower.y 
                                         //<< displayRegion.upper.x << " " << displayRegion.upper.y << std::endl;

                for (int iy=globalRegion.lower.y;iy<globalRegion.upper.y;iy++) {
                  
                  if (iy < displayRegion.lower.y) continue;
                  if (iy >= displayRegion.upper.y) continue;
                  
                  for (int ix=globalRegion.lower.x;ix<globalRegion.upper.x;ix++) {
                    if (ix < displayRegion.lower.x) continue;
                    if (ix >= displayRegion.upper.x) continue;
                    
                    const vec2i globalCoord(ix,iy);
                    const vec2i tileCoord = globalCoord-plain.region.lower;
                    const vec2i localCoord = globalCoord-displayRegion.lower;
                    const int localPitch = wallConfig.pixelsPerDisplay.x;
                    const int tilePitch  = plain.pitch;
                    const int tileOfs = tileCoord.x + tilePitch * tileCoord.y;
                    const int localOfs = localCoord.x + localPitch * localCoord.y;
                    localPixel[localOfs] = tilePixel[tileOfs];
                    ++numWritten;
                  }
                }

                {
#if THREADED_RECV
                  std::lock_guard<std::mutex> lock(displayMutex);
#endif
                  numWrittenThisFrame += numWritten;
                  DW_DBG(printf("written %li / %li\n",numWrittenThisFrame,numExpectedThisFrame));
                  if (numWrittenThisFrame == numExpectedThisFrame) {
                    //printf("display %i/%i has a full frame!\n",displayGroup.rank,displayGroup.size);
                   // need barrier here! if not, some images saved inside callback function are wrong. 
                    // displayGroup.barrier();
                    DW_DBG(printf("#osp:dw(%i/%i) barrier'ing on %i/%i\n",
                                  displayGroup.rank,displayGroup.size,
                                  outside.rank,outside.size));
                    DW_DBG(printf("#osp:dw(%i/%i): DISPLAYING\n",
                                  displayGroup.rank,displayGroup.size));
                    //MPI_CALL(Barrier(outside.comm));
                    displayCallback(recv_l,recv_r,objectForCallback);

                    // reset counter
                    numWrittenThisFrame = 0;
                    numExpectedThisFrame = wallConfig.displayPixelCount();
                    // and switch the in/out buffers
                    std::swap(recv_l,disp_l);
                    std::swap(recv_r,disp_r);
                    // Correct !
                  }
                }
              }
          }
          CompressedTile::freeDecompressor(decompressor);
// #if THREADED_RECV
        });
#endif
    }

  } // ::ospray::dw
} // ::ospray
