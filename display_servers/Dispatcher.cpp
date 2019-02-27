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
#include <algorithm>
#include <atomic>
#include <vector>
#include <iterator>

namespace ospray {
  namespace dw {

    using std::cout; 
    using std::endl;
    using std::flush;

   /* [>! the dispatcher that receives tiles on the head node, and then
      dispatches them to the actual tile receivers */
    void Server::runDispatcher()
    {
        for(int i = 0; i < clientNum; i++)
        {
            int sd = client_socket[i];
            int rank_index = rankList[i];
            //  std::cout << " socket #" << sd << " rank #" << rank_index 
            //                 << " numPixelsFromClient #" << numPixelsPerClient[rank_index] << std::endl;
            recvThread[i] = std::thread([i, rank_index, sd, this](){
                // std::cout << "start thread #" << i << std::endl;
                // std::cout << "socket #" << sd << std::endl;
                // std::cout << " num pixels from this client = " << numPixelsPerClient[i] << std::endl;
                std::vector<std::shared_ptr<CompressedTile>> inbox; 
                while (!quit_threads)
                {
                    // static std::atomic<int> tileID(0);
                    // int myTileID = tileID++;
                    auto tile = std::make_shared<CompressedTile>();
                    // receive the data size of compressed tile
                    int numBytes = 0;
                    auto start = std::chrono::high_resolution_clock::now();

                    int dataSize = recv(sd, &numBytes, sizeof(int), 0);
                    std::cout << "socket = " << sd << " Compressed tile would have " << numBytes << " bytes" << std::endl;
                    // std::cout << " data read " << dataSize << std::endl;
                    tile ->numBytes = numBytes;
                    tile ->data = new unsigned char[tile ->numBytes];
                    valread = recv(sd , tile->data, tile ->numBytes, MSG_WAITALL);

                    auto end = std::chrono::high_resolution_clock::now();
                    // std::cout << " tile ID = " << myTileID << " and num of bytes = " << valread << std::endl;
                    const box2i region = tile ->getRegion();

                    // std::cout << " frame ID = " << tile ->getFrameID() << std::endl; 
                    // std::cout << "Got region:  " << region << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(addMutex);
                        recvtimes.push_back(std::chrono::duration_cast<realTime>(end - start));
                        compressions.emplace_back( 100.0 * static_cast<float>(numBytes) / (tileSize * tileSize));
                    }
                    
                    // check if the frameID is equal to currFrameID
                    if((tile -> getFrameID()) == currFrameID){
                        // std::cout << "currFrameID = " << currFrameID << std::endl;
                        //const box2i region = message ->getRegion();
                        //std::cout << "socket " << sd << " processing region " << region << std::endl;
                        const box2i affectedDisplays = wallConfig.affectedDisplays(region);
                        for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
                            for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                                int toRank = wallConfig.rankOfDisplay(vec2i(dx,dy));
                                // printf("socket %i region %i %i - %i %i To rank #%i \n", sd,
                                //                                                         region.lower.x,
                                //                                                         region.lower.y,
                                //                                                         region.upper.x,
                                //                                                         region.upper.y,
                                //                                                         toRank);
                                MPI_CALL(Send(tile ->data, tile ->numBytes, MPI_BYTE, toRank, 0, displayGroup.comm));
                            }
                        {
                            std::lock_guard<std::mutex> lock(addMutex);
                            recvNumTiles++;
                        }
                    }else{
                        inbox.push_back(tile);
                    }

                    // go through inbox to see if there's a tile with the currFrameID
                    if(!inbox.empty()){
                        auto it = std::partition(inbox.begin(), inbox.end(), 
                                                    [&](const std::shared_ptr<CompressedTile> &t){
                                                    return t ->getFrameID() != currFrameID;
                                                });
                        for(auto m = it; m != inbox.end(); ++m){
                            {
                                std::lock_guard<std::mutex> lock(addMutex);
                                recvNumTiles++;
                            }
                            const box2i region = (*m) ->getRegion();
                            const box2i affectedDisplays = wallConfig.affectedDisplays(region);
                            for (int dy=affectedDisplays.lower.y;dy<affectedDisplays.upper.y;dy++)
                                for (int dx=affectedDisplays.lower.x;dx<affectedDisplays.upper.x;dx++) {
                                    int toRank = wallConfig.rankOfDisplay(vec2i(dx,dy));
                                    // printf("socket %i region %i %i - %i %i To rank #%i \n", sd,
                                    //                                                     region.lower.x,
                                    //                                                     region.lower.y,
                                    //                                                     region.upper.x,
                                    //                                                     region.upper.y,
                                    //                                                     toRank);
                                    MPI_CALL(Send((*m) ->data, (*m) ->numBytes, MPI_BYTE, toRank, 0, displayGroup.comm));
                            }
                        }
                        // erase all sent tiles
                        inbox.erase(it, inbox.end());
                    }

                    { 
                        std::lock_guard<std::mutex> lock(addMutex);
                        // if we have received all tiles 
                        if(recvNumTiles == numTilesPerFrame){
                            recvNumTiles = 0;
                            currFrameID++;
                            // realTime sumTime;
                            // for(size_t i = 0; i < recvtimes.size(); i++){
                            //     sumTime += recvtimes[i];
                            // }
                            // recvTime.push_back(std::chrono::duration_cast<realTime>(sumTime));
                            // recvtimes.clear();
                        }
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
                        // endFramePrint();
                    }
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
