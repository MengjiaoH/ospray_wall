#pragma once

#include "mpiCommon/MPICommon.h"
#include "ospcommon/networking/Socket.h"
#include "bench/pico_bench/pico_bench.h"
#include "../common/WallConfig.h"
#include "../common/CompressedTile.h"
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <chrono>

namespace ospray {
  namespace dw {
        typedef void (*DisplayCallback)(const uint32_t *leftEye, 
                                        const uint32_t *rightEye,
                                        void *objects);
        
        class Server{
            public:
                static Server *singleton;
                Server(const int &portNum,
                       const mpicommon::Group &me,
                       const mpicommon::Group &displayGroup,
                       const mpicommon::Group &dispatchGroup,
                       const WallConfig &wallConfig,
                       DisplayCallback displayCallback,
                       void *objectForCallback,
                       const bool &hasHeadNode,
                       const int &ppn,
                       const int clientNum);
                void setupCommunication(); 
                void processIncomingTiles(mpicommon::Group &ourside);
                void allocateFrameBuffers();
                void runDispatcher(const mpicommon::Group &outsideClients,
                                   const mpicommon::Group &displayGroup,
                                   const WallConfig &wallConfig,
                                   const bool &use_tcp);
            private:
                const int portNum;
                
                int service_sock;
                int valread;
                int client_socket[30];
                fd_set readfds;
                int max_sd, sd;
                int max_clients = 30;
                struct sockaddr_in address;
                int addrlen;
                std::mutex recvMutex;

                const int ppn;
                int clientNum;

                static std::thread commThread;
                /*! group that contails ALL display service procs, including the
                    head node (if applicable) */
                const mpicommon::Group me;
                /*! group that contains only the display procs; either a
                    intracomm (if a display node), or the intercomm (if head
                    node) */
                mpicommon::Group displayGroup;
                mpicommon::Group dispatchGroup;

                const WallConfig wallConfig;
                const bool hasHeadNode;
      
                const DisplayCallback displayCallback;
                void *const objectForCallback;

                /*! total number of pixels already written this frame */
                size_t numWrittenThisFrame;
                /*! total number of pixels we have to write this frame until we
                    have a full frame buffer */
                size_t numExpectedThisFrame;
                /*! @{ the four pixel arrays for left/right eye and
                    receive/display buffers, respectively */
                uint32_t *recv_l, *recv_r, *disp_l, *disp_r;
                /*! @} */

                // Measurement 
                size_t tileSize = 256;
                size_t numBytesAfterCompression = 0;
                // compression ratio
                using compressionPercent = std::chrono::duration<float>;
                std::vector<compressionPercent> compressions;
                // Compression time
                using realTime = std::chrono::duration<double, std::milli>;
                std::vector<realTime> recvtimes, decompressiontimes;
                std::vector<realTime> decompressionTime, recvTime, sendTime;


                using compressionStats = pico_bench::Statistics<compressionPercent>;
                using Stats = pico_bench::Statistics<realTime>;
                mpicommon::Group waitForConnection(const mpicommon::Group &outFacingGroup,
                                       const int &portNum);        

                void sendConfigToClient(const mpicommon::Group &outside, 
                                        const mpicommon::Group &me,
                                        const WallConfig &wallConfig);
                void endFramePrint();

        };
        void startWallServer(const mpicommon::Group world,
                                    const mpicommon::Group displayGroup,
                                    const mpicommon::Group dispatchGroup,
                                    const WallConfig &wallConfig,
                                    bool hasHeadNode,
                                    DisplayCallback displayCallback,
                                    void *objectForCallback,
                                    int portNum,
                                    int process_pernode,
                                    int clientNum);

  }// ospray::dw
}//ospray
