
#pragma once
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ospcommon/vec.h"
#include "mpiCommon/MPICommon.h"
#include "../common/WallConfig.h"

namespace ospray{
    namespace dw{
        struct ServiceInfo {
            /* constructor that initializes everything to default values */
            ServiceInfo(): mpiPortName("<value not set>"),
                                  wallInfo(NULL)
            {}

          /*! total pixels in the entire display wall, across all
            indvididual displays, and including bezels (future versios
            will allow to render to smaller resolutions, too - and have
            the clients upscale this - but for now the client(s) have to
            render at exactly this resolution */

          WallInfo *wallInfo;
          /*! the MPI port name that the service is listening on client
              connections for (ie, the one to use with
              client::establishConnection) */
          std::string mpiPortName;

          int sock = 0;

          /*! read a service info from a given hostName:port. The service
            has to already be running on that port 

            Note this may throw a std::runtime_error if the connection
            cannot be established 
          */
          void getFrom(const std::string &hostName,
                       const int portNo);

          void sendTo(WallInfo &wallInfo, const std::string &hostName, int &clientNum);

          // void constructWallConfig(
        };

}// end of ospray::dw
}//end of ospray
