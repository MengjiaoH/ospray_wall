// c++
#include <iostream>
#include <mutex>
#include <atomic>
#include <memory>
#include <sys/socket.h>
#include <arpa/inet.h>
//common libs
#include "../common/WallConfig.h"
#include "../common/helper.h"
#include "../common/CompressedTile.h"

// ospray
#include "ospcommon/vec.h"
#include "mpiCommon/MPICommon.h"
#include "ospcommon/containers/TransactionalBuffer.h"
#include "ospcommon/networking/Socket.h"
#include "bench/pico_bench/pico_bench.h"

namespace ospray{
    namespace dw{
        
        using namespace ospcommon;
        using Message = mpicommon::Message;

        class Client{
            public:
            
                Client(const std::string &hostName, 
                       const int &portNum,
                       const mpicommon::Group &me,
                       const WallInfo *wallinfo);
                
                ~Client();
                
                vec2i totalPixelsInWall() const;
                const WallConfig *getWallConfig() const{ return wallConfig; }
                void writeTile(const PlainTile &tile);
                void sendTile();
                void endFrame();
                void printStatistics();
                            
            private: 
                // host name and port number connect to
                std::string hostName;
                int sock;
                int portNum;
                std::mutex sendMutex;

                // MPI Group
                mpicommon::Group me;
                mpicommon::Group displayGroup;
                // wall config
                WallConfig *wallConfig;
                WallInfo *wallInfo;
 
                size_t numWrittenThisFrame = 0;
                size_t numExpectedThisFrame;          
                ospcommon::TransactionalBuffer<std::shared_ptr<Message>> outbox;
 
                // Measurement
                
                using realTime = std::chrono::duration<double, std::milli>;
                std::vector<realTime> sendtimes, compressiontimes;
                std::vector<realTime> sendTime, compressionTime;
                using Stats = pico_bench::Statistics<realTime>;

                void establishConnection();
                void constructWallConfig(const WallInfo *wallinfo );
        };
    }
}
