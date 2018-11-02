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
    
    static size_t numWrittenThisFrame = 0;
    //std::mutex addMutex;
    //static ospcommon::TransactionalBuffer<std::shared_ptr<CompressedTile>> inbox;

   /* [>! the dispatcher that receives tiles on the head node, and then
      dispatches them to the actual tile receivers */
    void Server::runDispatcher()
    {
        // size_t numWrittenThisFrame = 0;
        // size_t numExpectedThisFrame = wallConfig.totalPixelCount();
        // std::cout << "socket index = " << socket_index <<std::endl;
        //std::cout << " ========== start receiving tiles ========== " << std::endl;
        for(int i = 0; i < clientNum; i++)
        {
            int sd = client_socket[i];
            if(sd > max_sd){
                max_sd = sd;
            }
            recvThread[i] = std::thread([i, sd, this](){
                std::cout << "start thread #" << i << std::endl;
                std::cout << "socket #" << sd << std::endl;
                std::vector<std::shared_ptr<CompressedTile>> inbox;
                while (!quit_threads)
                {
                    // fd_set readfds_local;
                    // FD_ZERO(&readfds);
                    // FD_SET(sd, &readfds);
                    // int activity = select(1, &readfds, NULL, NULL, NULL);
                    // if((activity < 0) && (errno != EINTR)){
                    //     printf("select error");
                    // }
                    // if (FD_ISSET(sd , &readfds))
                    // {
            
                        // std::cout << "file set " << FD_ISSET( sd , &readfds) << " on socket #" << sd << std::endl;
                        static std::atomic<int> tileID(0);
                        int myTileID = tileID++;
                        auto tile = std::make_shared<CompressedTile>();
                        // receive the data size of compressed tile
                        int numBytes = 0;
                        auto start = std::chrono::high_resolution_clock::now();

                        int dataSize = recv(sd, &numBytes, sizeof(int), MSG_WAITALL);
                        // std::cout << " Compressed tile would have " << numBytes << " bytes" << std::endl;
                        // std::cout << " data read " << dataSize << std::endl;
                        tile ->numBytes = numBytes;
                        tile ->data = new unsigned char[tile ->numBytes];
                        valread = recv( sd , tile->data, tile ->numBytes, MSG_WAITALL);

                        auto end = std::chrono::high_resolution_clock::now();
                        // std::cout << " tile ID = " << myTileID << " and num of bytes = " << valread << std::endl;
                        const box2i region = tile ->getRegion();
                        // std::cout << "Got region:  " << region << std::endl;
                        {
                            std::lock_guard<std::mutex> lock(addMutex);
                            recvtimes.push_back(std::chrono::duration_cast<realTime>(end - start));
                            compressions.emplace_back( 100.0 * static_cast<float>(numBytes) / (tileSize * tileSize));
                        }


                        // std::lock_guard<std::mutex> lock(recvMutex);
                        // numWrittenThisFrame += region.size().product();
                        numWrittenThisClient[i] += region.size().product();
                        // push the tile to the inbox
                        inbox.push_back(tile);

                        if(numWrittenThisClient[i] == numPixelsPerClient[i]){
                            if(!inbox.empty()){
                                    for (auto &message : inbox) {
                                            const box2i region = message ->getRegion();
                                            std::cout << "socket " << sd << " processing region " << region << std::endl;
                                            const box2i affectedDisplays = wallConfig.affectedDisplays(region);
                                        for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
                                            for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                                                int toRank = wallConfig.rankOfDisplay(vec2i(dx,dy));
                                                // printf("socket %i region %i %i - %i %i To rank #%i \n", sd,
                                                //                                                                 region.lower.x,
                                                //                                                                 region.lower.y,
                                                //                                                                 region.upper.x,
                                                //                                                                 region.upper.y,
                                                //                                                                 toRank);
                                                MPI_CALL(Send(message ->data, message ->numBytes, MPI_BYTE, toRank, myTileID, displayGroup.comm));
                                    }
                                }
                            }
                            numWrittenThisClient[i] = 0;
                            inbox.clear();
                        }

                        if (valread == 0)
                        {
                            quit_threads = true;
                            //Somebody disconnected , get his details and print
                            getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                            printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                            //Close the socket and mark as 0 in list for reuse
                            close( sd );
                            client_socket[i] = 0;
                            endFramePrint();
                        }
                //    } // end of fd_isset
                }
            });
       }// end of for loop
        for(int i = 0; i < clientNum; i++){
                recvThread[i].join();
                std::cout << "thread #" << i << " join " << "\n";
        }
    }// end of runDispatcher

  } // ::ospray::dw
} // ::ospray
