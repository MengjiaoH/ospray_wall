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
    void Server::runDispatcher(int socket_index, fd_set readfds)
    {

        size_t numWrittenThisFrame = 0;
        size_t numExpectedThisFrame = wallConfig.totalPixelCount();
         while (1) {
            //std::cout << " ========== start receiving tiles ========== " << std::endl;
            //sd = client_socket[socket_index];
            sd = socket_index;
            std::cout << "socket " << sd << " is set " << FD_ISSET( sd , &readfds) << std::endl;
            if (FD_ISSET( sd , &readfds)){
                static std::atomic<int> tileID;
                int myTileID = tileID++;
                CompressedTile encoded;
                // receive the data size of compressed tile
                int numBytes = 0;
                auto start = std::chrono::high_resolution_clock::now();

                int dataSize = recv(sd, &numBytes, sizeof(int), 0 );
                std::lock_guard<std::mutex> lock(recvMutex);
                std::cout << " Compressed tile would have " << numBytes << " bytes" << std::endl;
                encoded.numBytes = numBytes;
                encoded.data = new unsigned char[encoded.numBytes];
                box2i region;
                // std::lock_guard<std::mutex> lock(recvMutex);
                valread = recv( sd , encoded.data, encoded.numBytes, MSG_WAITALL);

                auto end = std::chrono::high_resolution_clock::now();
                // std::cout << " tile ID = " << myTileID << " and num of bytes = " << valread << std::endl;
                region = encoded.getRegion();
                recvtimes.push_back(std::chrono::duration_cast<realTime>(end - start));
                // compressions.emplace_back( 100.0 * static_cast<float>(numBytes) / (tileSize * tileSize));
                            
                const box2i affectedDisplays = wallConfig.affectedDisplays(region);

                printf("region %i %i - %i %i displays %i %i - %i %i\n",
                        region.lower.x,
                        region.lower.y,
                        region.upper.x,
                        region.upper.y,
                        affectedDisplays.lower.x,
                        affectedDisplays.lower.y,
                        affectedDisplays.upper.x,
                        affectedDisplays.upper.y);

                for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
                    for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                        int toRank = wallConfig.rankOfDisplay(vec2i(dx,dy));
                 //       MPI_CALL(Send(encoded.data, encoded.numBytes, MPI_BYTE, toRank, myTileID, displayGroup.comm));
                }

                numWrittenThisFrame += region.size().product();
                numBytesAfterCompression += encoded.numBytes;
                            
                //printf("dispatch %i/%i\n",numWrittenThisFrame,numExpectedThisFrame);

                if (numWrittenThisFrame == numExpectedThisFrame) {
                    // printf("Compression ratio = %d\n", numBytesAfterCompression ); 
                    // printf("#osp:dw(hn): head node has a full frame\n");
                    numWrittenThisFrame = 0;
                    numBytesAfterCompression = 0;
                    realTime sum_recv;
                    for(size_t i = 0; i < recvtimes.size(); i++){
                        sum_recv += recvtimes[i];
                    }
                    recvTime.push_back(std::chrono::duration_cast<realTime>(sum_recv));
                    recvtimes.clear();
                    dispatchGroup.barrier();
                    recvThreads[socket_index].join();
                    // endFramePrint();
                }

                if (valread == 0){
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[socket_index] = 0;
                    //endFramePrint();
                    // recvThreads[socket_index].join();
                }
            } // end of if(FD_ISSET)

         }// end of while
        
    }// end of runDispatcher
    
  } // ::ospray::dw
} // ::ospray
