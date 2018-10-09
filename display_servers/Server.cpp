#include "Server.h"
#include <pthread.h>
#include <atomic>

namespace ospray{
    namespace dw{
        Server *Server::singleton = NULL;
        std::thread Server::commThread;

        Server::Server(const int &portNum,
                       const mpicommon::Group &me,
                       const mpicommon::Group &displayGroup,
                       const mpicommon::Group &dispatchGroup,
                       const WallConfig &wallConfig,
                       DisplayCallback displayCallback,
                       void *objectForCallback,
                       const bool &hasHeadNode,
                       const int &ppn,
                       const int clientNum)
            :portNum(portNum), me(me), displayGroup(displayGroup),
             dispatchGroup(dispatchGroup), wallConfig(wallConfig),
             displayCallback(displayCallback), objectForCallback(objectForCallback),
             hasHeadNode(hasHeadNode), ppn(ppn), 
             numWrittenThisFrame(0), numExpectedThisFrame(wallConfig.displayPixelCount()),
             recv_l(NULL), recv_r(NULL), disp_l(NULL), disp_r(NULL), clientNum(clientNum)
        {
            commThread = std::thread([&](){
                    setupCommunication();
            });
                    
            if(hasHeadNode && me.rank == 0){
                commThread.join();
            }
        }

        void Server::setupCommunication()
        {
            const mpicommon::Group &world = me;
            //std::cout << "Using TCP/IP port" << std::endl;
            //std::cout << "debug display Group size = " << displayGroup.size << std::endl;
            //std::cout << "debug dispatch Group size = " << dispatchGroup.size << std::endl;
            if(hasHeadNode){
                // =======================================================
                // DISPATCHER
                // =======================================================
                if(world.rank == 0){
                    mpicommon::Group outsideConnection = waitForConnection(dispatchGroup, portNum);
                    //runDispatcher(dispatchGroup, displayGroup, wallConfig, use_tcp);
                }else{
                    // =======================================================
                    // TILE RECEIVER
                    // =======================================================
                     //canStartProcessing.lock();
                    mpicommon::Group incomingTiles = dispatchGroup;
                    processIncomingTiles(incomingTiles);
                }
            }
        }

        mpicommon::Group Server::waitForConnection(const mpicommon::Group &outwardFacingGroup,
                                                   const int &portNum)
        {
            char portName[MPI_MAX_PORT_NAME];
            MPI_Comm outside;
            if (outwardFacingGroup.rank == 0) { 
                std::cout << std::endl;
                int opt = 1;
                int activity;
                int new_socket;
                
                // Set of socket descriptors
                addrlen = sizeof(address);
                // Creating socket file descriptor
                if ((service_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0){
                    perror("socket failed");
                    exit(EXIT_FAILURE);
                }
                // Forcefully attaching socket to the port 8080
                if (setsockopt(service_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
                {
                    perror("setsockopt");
                    exit(EXIT_FAILURE);
                }
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = INADDR_ANY;
                address.sin_port = htons(portNum);
                // Forcefully attaching socket to the port 8080
                if (bind(service_sock, (struct sockaddr *)&address, sizeof(address))<0)
                {
                    perror("bind failed");
                    exit(EXIT_FAILURE);
                }
                if (listen(service_sock, 30) < 0)
                {
                    perror("listen");
                    exit(EXIT_FAILURE);
                }
                    
                for (int i = 0; i < max_clients; i++)
                {
                    client_socket[i] = 0;
                }
                // Accept incoming connections
                printf("Waiting for connections ... \n");
                
                while(1)
                {
                    //clear the socket set
                    FD_ZERO(&readfds);
                    // add master socket to set
                    FD_SET(service_sock, &readfds);
                    max_sd = service_sock;
                    // add child sockets to set
                    for( int i = 0; i < max_clients; i++){
                        // socket descriptor
                        sd = client_socket[i];
                        // if valid socket descroptor then add to read list
                        if(sd > 0){
                            FD_SET(sd, &readfds);
                        }
                        // highest file descriptor number, need it for the select function
                        if(sd > max_sd){
                            max_sd = sd;
                        }
                    }
                    
                    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
                    if((activity < 0) && (errno != EINTR)){
                        printf("select error");
                    }
                    //If something happened on the master socket , 
                    //then its an incoming connection 
                    if (FD_ISSET(service_sock, &readfds))  
                    {  
                        if ((new_socket = accept(service_sock, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
                        {
                            perror("accept");  
                            exit(EXIT_FAILURE);  
                        } 
                        //inform user of socket number - used in send and receive commands 
                        printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs
                               (address.sin_port));
                        //add new socket to array of sockets 
                        for (int i = 0; i < max_clients; i++)  {  
                            //if position is empty 
                            if( client_socket[i] == 0 )  
                            {
                                client_socket[i] = new_socket;  
                                printf("Adding to list of sockets as %d\n" , i);
                                break;
                            }
                        }
                    }
                 
                    //! Start receiving tiles from outside
                    // TODO: Try to move it to another method
                    // TODO: Push tiles in a inbox 
                    // TODO: Send out tiles in a separate thread
                    for (int i = 0; i < clientNum; i++)
                    {
                        sd = client_socket[i];

                        if (FD_ISSET( sd , &readfds)){
                            static std::atomic<int> tileID;
                            int myTileID = tileID++;
                            CompressedTile encoded;
                            // receive the data size of compressed tile
                            int numBytes = 0;
                            int dataSize = recv(sd, &numBytes, sizeof(int), 0 );
                            std::cout << " Compressed tile would have " << numBytes << " bytes" << std::endl;
                            encoded.numBytes = numBytes;
                            encoded.data = new unsigned char[encoded.numBytes];
                            box2i region;

                            std::lock_guard<std::mutex> lock(recvMutex);
                            valread = recv( sd , encoded.data, encoded.numBytes, MSG_WAITALL);
                            std::cout << " tile ID = " << myTileID << " and num of bytes = " << valread << std::endl;
                            region = encoded.getRegion();

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
                                    MPI_CALL(Send(encoded.data, encoded.numBytes, MPI_BYTE, toRank, myTileID, displayGroup.comm));
                            }

                            numWrittenThisFrame += region.size().product();

                            //printf("dispatch %i/%i\n",numWrittenThisFrame,numExpectedThisFrame);

                            if (numWrittenThisFrame == numExpectedThisFrame) {
                                // printf("#osp:dw(hn): head node has a full frame\n");
                                dispatchGroup.barrier();
                                numWrittenThisFrame = 0;
                            }

                            if (valread == 0){
                                //Somebody disconnected , get his details and print
                                getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                                //Close the socket and mark as 0 in list for reuse
                                close( sd );
                                client_socket[i] = 0;
                            }
                    }
                }
            }
            outwardFacingGroup.barrier();
            return outwardFacingGroup;
        }
        }

        void Server::allocateFrameBuffers()
        {
          assert(recv_l == NULL);
          assert(disp_l == NULL);
          assert(recv_r == NULL);
          assert(disp_r == NULL);

          const int pixelsPerBuffer = wallConfig.pixelsPerDisplay.product();
          recv_l = new uint32_t[pixelsPerBuffer];
          disp_l = new uint32_t[pixelsPerBuffer];
          if (wallConfig.stereo) {
            recv_r = new uint32_t[pixelsPerBuffer];
            disp_r = new uint32_t[pixelsPerBuffer];
          }
        }// end of allocateFrameBuffers


        void startWallServer(const mpicommon::Group world,
                                    const mpicommon::Group displayGroup,
                                    const mpicommon::Group dispatchGroup,
                                    const WallConfig &wallConfig,
                                    bool hasHeadNode,
                                    DisplayCallback displayCallback,
                                    void *objectForCallback,
                                    int portNum,
                                    int process_pernode,
                                    int clientNum){
            assert(Server::singleton == NULL);
            Server::singleton = new Server(portNum, world, displayGroup, dispatchGroup, wallConfig, displayCallback, objectForCallback, hasHeadNode, process_pernode, clientNum);
        }
 

}// end of ospray::dw
}// end of ospray
