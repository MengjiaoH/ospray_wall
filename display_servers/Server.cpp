#include "Server.h"
#include <pthread.h>
#include <atomic>
#include <chrono>
#include <thread>

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
             numWrittenThisFrame(0), numExpectedThisFrame(wallConfig.totalPixels().product()),
             recv_l(NULL), recv_r(NULL), disp_l(NULL), disp_r(NULL), clientNum(clientNum)
        {
            // commThread = std::thread([&](){
                    setupCommunication();
            // });
                    
            // if(hasHeadNode && me.rank == 0){
            //     commThread.join();
            // }
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
                    // for(int i = 0; i < clientNum; i++){
                            // printf("Start a new thread handling incoming tiles for socket #%d ..", client_socket[i]);
                            // recvThreads[i] = std::thread([i, this]{
                                // printf("debug threading\n");
                                //runDispatcher(i);
                                // });
                            // recvThreads[i].detach();
                    // }
                    //runDispatcher(dispatchGroup, displayGroup, wallConfig, use_tcp);
                }else{
                    // =======================================================
                    // TILE RECEIVER
                    // =======================================================
                     //canStartProcessing.lock();
                    mpicommon::Group incomingTiles = dispatchGroup;
                    //processIncomingTiles(incomingTiles);
                }
                // for(int i = 0; i < clientNum; i++){
                //     std::cout << "Joining a thread .." << std::endl;
                //     recvThreads[i].join();
                // }
            }
        }

        mpicommon::Group Server::waitForConnection(const mpicommon::Group &outwardFacingGroup,
                                                   const int &portNum)
        {
            char portName[MPI_MAX_PORT_NAME];
            MPI_Comm outside;
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
                    
                // for (int i = 0; i < max_clients; i++)
                // {
                //     client_socket[i] = 0;
                // }
                // Accept incoming connections
                printf("Waiting for connections ... \n");
                int ii = 0;
                 while(1)
                // for(int s = 0; s < clientNum; s++)
                {
                    // std::cout << "DEBUG " << std::endl;
                    // //clear the socket set
                    FD_ZERO(&readfds);
                    // // add master socket to set
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
                    
                    // activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
                    // if((activity < 0) && (errno != EINTR)){
                    //     printf("select error");
                    // }
                    // //If something happened on the master socket , 
                    // //then its an incoming connection 
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
                        recvThreads[ii] = std::thread(&Server::runDispatcher, this, new_socket, readfds);
                        // recvThreads[ii] = std::thread([new_socket, readfds, this]{
                        //         runDispatcher(new_socket, readfds);
                                // });
                        //recvThreads[s].detach();
                        std::cout << "ii == " << ii << std::endl;
                        ii++;
                    }
                } // end of while
            //    for( int  s = 0; s  < clientNum; s++ ){
            //                     std::cout << "join thread " << std::endl;
            //                     recvThreads[s].join();
            //                 } 
            outwardFacingGroup.barrier();
            return outwardFacingGroup;
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

        void Server::endFramePrint()
        {
            compressionStats stats(compressions);
            stats.time_suffix = "%";
            std::cout << "Compression ratio statistics: \n" << stats << "\n";

            if(!recvTime.empty()){
                Stats recvStats(recvTime);
                recvStats.time_suffix = "ms";
                std::cout  << "Message recv statistics:\n" << recvStats << "\n";
            }
            //std::cout << "decompression is " << decompressionTime.empty() << std::endl;

            // if(!decompressionTime.empty()){
            //     Stats decompressionStats(decompressionTime);
            //     decompressionStats.time_suffix = "ms";
            //     std::cout  << "Decompression time statistics:\n" << decompressionStats << "\n";
            // }

        }
 

}// end of ospray::dw
}// end of ospray
