/* 
   Copyright (c) 2016 Ingo Wald

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include "../common/CompressedTile.h"
#include "../common/WallConfig.h"
#include "../common/helper.h"
#include "Server.h"
#include <atomic>
#include <vector>

namespace ospray {
  namespace dw {

    using std::cout; 
    using std::endl;
    using std::flush;

   /* [>! the dispatcher that receives tiles on the head node, and then
      dispatches them to the actual tile receivers */
    void Server::runDispatcher(const mpicommon::Group &outsideClients,
                       const mpicommon::Group &displayGroup,
                       const WallConfig &wallConfig,
                       const bool &use_tcp)
    {
        if(!use_tcp){
          // std::thread *dispatcherThread = new std::thread([=]() {
          std::cout << "#osp:dw(hn): running dispatcher on rank 0" << std::endl;

          size_t numWrittenThisFrame = 0;
          size_t numExpectedThisFrame = wallConfig.totalPixelCount();

          while (1) {
            CompressedTile encoded;
            DW_DBG(printf("dispatcher trying to receive...\n"));
            encoded.receiveOne(outsideClients);
            
            static std::atomic<int> ID;
            int myTileID = ID++;
            std::cout << "tile ID = " << ID << std::endl;

            //CompressedTileHeader *header = (CompressedTileHeader *)encoded.data;
            //std::string filename = "MPI_afterDecompress_"          + std::to_string(header ->region.lower.x) + "_"
                                                                   //+ std::to_string(header ->region.lower.y) + "_"
                                                                   //+ std::to_string(header ->region.upper.x) + "_"
                                                                   //+ std::to_string(header ->region.upper.y) + ".ppm";
            //ospcommon::vec2i img_size(512, 288);
            //writePPM(filename.c_str(), &img_size, (uint32_t*)header -> payload);


            const box2i region = encoded.getRegion();
            //printf("region %i %i - %i %i",
                          //region.lower.x,
                          //region.lower.y,
                          //region.upper.x,
                          //region.upper.y);

            
            // -------------------------------------------------------
            // compute displays affected by this tile
            // -------------------------------------------------------
            const box2i affectedDisplays = wallConfig.affectedDisplays(region);
            
            //printf("region %i %i - %i %i displays %i %i - %i %i\n",
                          //region.lower.x,
                          //region.lower.y,
                          //region.upper.x,
                          //region.upper.y,
                          //affectedDisplays.lower.x,
                          //affectedDisplays.lower.y,
                          //affectedDisplays.upper.x,
                          //affectedDisplays.upper.y);

            // -------------------------------------------------------
            // now, send to all affected displays ...
            // -------------------------------------------------------
            for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
              for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                // printf("sending to %i/%i -> %i\n",dx,dy,wallConfig.rankOfDisplay(vec2i(dx,dy)));
                encoded.sendTo(displayGroup,wallConfig.rankOfDisplay(vec2i(dx,dy)));
              }

            numWrittenThisFrame += region.size().product();
            DW_DBG(printf("dispatch %i/%i\n",numWrittenThisFrame,numExpectedThisFrame));
            if (numWrittenThisFrame == numExpectedThisFrame) {
              DW_DBG(printf("#osp:dw(hn): head node has a full frame\n"));
              outsideClients.barrier();
              displayGroup.barrier();

              numWrittenThisFrame = 0;
            }
          };
        }else{
            size_t numWrittenThisFrame = 0;
            size_t numExpectedThisFrame = wallConfig.totalPixelCount();
            int tileSize = 256;
            size_t max_numBytes = tileSize * tileSize * 4 + sizeof(CompressedTileHeader);

            while (1) {
                //  std::cout << " ========== start receiving tiles ========== " << std::endl;
                 for (int i = 0; i < 30; i++)
                 {
                    sd = client_socket[i];

                    if (FD_ISSET( sd , &readfds)){
                      CompressedTile encoded;
                      encoded.data = new unsigned char[max_numBytes];
                        //Check if it was for closing , and also read the
                       //incoming message
                       encoded.numBytes = max_numBytes;

                       valread = recv( sd , encoded.data, max_numBytes, 0);

                      box2i region = encoded.getRegion();
                     static std::atomic<int> tileID;
                     int myTileID = tileID++;

                     const box2i affectedDisplays = wallConfig.affectedDisplays(region);
                     std:;cout << " tile ID = " << myTileID << " and num of bytes = " << encoded.numBytes << std::endl;
                     printf("region %i %i - %i %i displays %i %i - %i %i\n",
                            region.lower.x,
                            region.lower.y,
                            region.upper.x,
                            region.upper.y,
                            affectedDisplays.lower.x,
                            affectedDisplays.lower.y,
                            affectedDisplays.upper.x,
                            affectedDisplays.upper.y);

                      if (valread == 0){
                        //Somebody disconnected , get his details and print
                        getpeername(sd , (struct sockaddr*)&address , \
                                                      (socklen_t*)&addrlen);
                        printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                        //Close the socket and mark as 0 in list for reuse
                        close( sd );
                        client_socket[i] = 0;
                        }
                      }
                    }

                //  for(int i = 0; i < client_connections.size(); i++){
                    //  CompressedTile encoded;
                    //  encoded.data = new unsigned char[max_numBytes];
                    //  //unsigned char *data_received = new unsigned char[max_numBytes];
                    //  read(client_connections[i], encoded.data, max_numBytes);
                    //  
                   
                    // for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
                    //     for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                    //         int toRank = wallConfig.rankOfDisplay(vec2i(dx,dy));
                    //         //std::cout << "to Rank = " << toRank << std::endl;
                    //         MPI_CALL(Send(encoded.data, max_numBytes, MPI_BYTE, toRank, myTileID, displayGroup.comm));
                    // }
                    
                    // numWrittenThisFrame += region.size().product();
                 
                    // //printf("dispatch %i/%i\n",numWrittenThisFrame,numExpectedThisFrame);
                    
                    // if (numWrittenThisFrame == numExpectedThisFrame) {
                    //     printf("#osp:dw(hn): head node has a full frame\n");
                    //     //displayGroup.barrier();
                    //     //outsideClients.barrier();
                    //     numWrittenThisFrame = 0;
                    //     //break;
                    // }    
                    //delete[] data_received;
                //  }
                 
                    
                 

                 //std::string filename = "service_afterReceive" + std::to_string(header ->region.lower.x) + "_"
                                                                  //+ std::to_string(header ->region.lower.y) + "_"
                                                                  //+ std::to_string(header ->region.upper.x) + "_"
                                                                  //+ std::to_string(header ->region.upper.y) + ".ppm";
                    //ospcommon::vec2i img_size(region.upper.x - region.lower.x, region.upper.y - region.lower.y);
                    //writePPM(filename.c_str(), &img_size, (uint32_t*)header -> payload);
               }// end of while
}// end of if else
}// end of runDispatcher
    
  } // ::ospray::dw
} // ::ospray
