
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
    void Server::processIncomingTiles(mpicommon::Group &outside)
    {
      allocateFrameBuffers();
      const box2i displayRegion = wallConfig.regionOfRank(displayGroup.rank);

#define THREADED_RECV 3

#if THREADED_RECV
      std::mutex displayMutex;
      // tasking::parallel_for(THREADED_RECV, [&](int) {
              void *decompressor = CompressedTile::createDecompressor();
              while (1) 
              {
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
                  auto start = std::chrono::high_resolution_clock::now();
                  encoded.decode(decompressor,plain);
                  auto end = std::chrono::high_resolution_clock::now();
                  decompressiontimes.push_back(std::chrono::duration_cast<realTime>(end - start));
                  //static std::atomic<int> tileID;
                  //int myTileID = tileID++;
                  //std::cout << "tile ID = " << tileId << std::endl;
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

#if THREADED_RECV
                  std::lock_guard<std::mutex> lock(displayMutex);
#endif
                  numHasWritten += numWritten;
                  printf("display # %d written %li / %li\n", displayGroup.rank,numHasWritten,numExpectedPerDisplay);
                  if (numHasWritten == numExpectedPerDisplay) {

                      // printf("display %i/%i has a full frame!\n",displayGroup.rank,displayGroup.size);
                    // need barrier here! if not, some images saved inside callback function are wrong. 
                      
                      numHasWritten = 0;
                      displayGroup.barrier();
                      // printf("display %i/%i has a full frame!\n", displayGroup.rank,displayGroup.size);
                      //need barrier here! if not, some images saved inside callback function are wrong.
                      realTime sumTime;
                      for(size_t i = 0; i < decompressiontimes.size(); i++){
                        sumTime += decompressiontimes[i];
                      }
                      decompressionTime.push_back(std::chrono::duration_cast<realTime>(sumTime));
                      decompressiontimes.clear();
                      //if(!decompressionTime.empty()){
                          //Stats decompressionStats(decompressionTime);
                          //decompressionStats.time_suffix = "ms";
                          //std::cout  << "Decompression time statistics:\n" << decompressionStats << "\n";
                      //}
                      // displayGroup.barrier();
                      DW_DBG(printf("#osp:dw(%i/%i) barrier'ing on %i/%i\n",
                                    displayGroup.rank,displayGroup.size,
                                    outside.rank,outside.size));
                      DW_DBG(printf("#osp:dw(%i/%i): DISPLAYING\n",
                                    displayGroup.rank,displayGroup.size));
                      displayCallback(recv_l,recv_r,objectForCallback);

                      // reset counter
                      // numExpectedPerDisplay = wallConfig.displayPixelCount();
                      // and switch the in/out buffers
                      std::swap(recv_l,disp_l);
                      std::swap(recv_r,disp_r);
                    // Correct !
                  }
                }
          CompressedTile::freeDecompressor(decompressor);
        // });
#endif
    }

  } // ::ospray::dw
} // ::ospray
