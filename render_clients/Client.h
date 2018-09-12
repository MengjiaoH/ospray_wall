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

namespace ospray{
    namespace dw{
        
        using namespace ospcommon;
        using Message = mpicommon::Message;

        class Client{
            public:
                Client(const std::string &hostName, 
                       const int &portNum,
                       const mpicommon::Group &me,
                       const bool use_tcp,
                       const WallInfo *wallinfo);
                ~Client();
                
                std::atomic<bool> quit_state;
                vec2i totalPixelsInWall() const;
                const WallConfig *getWallConfig() const{ return wallConfig; }

                void writeTile(const PlainTile &tile);
                void sendTile();
                void endFrame();
                            
            private: 
                // host name and port number connect to
                std::string hostName;
                int portNum;
                int sock;

                // MPI Group
                mpicommon::Group me;
                mpicommon::Group displayGroup;
                // wall config
                WallConfig *wallConfig;
                WallInfo *wallInfo;
                
                // check for received 
                bool hasMetaData;
                bool use_tcp;
                std::mutex sendMutex;
                std::mutex state_mutex;

                ospcommon::TransactionalBuffer<std::shared_ptr<Message>> outbox;
                
                void establishConnection();
                void receiveWallConfig(const WallInfo *wallinfo );
        };
    }
}