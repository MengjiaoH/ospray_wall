#include "Client.h"
#include <atomic>
#include <thread>

namespace ospray{
    namespace dw{
        
        template<typename T, typename... Args>
            std::unique_ptr<T> make_unique(Args&&... args) {

            return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        }

        Client::Client(const std::string &hostName,
                       const int &portNum,
                       const mpicommon::Group &me,
                       const bool use_tcp,
                       const WallInfo *wallinfo)
            :hostName(hostName), portNum(portNum),
             me(me), hasMetaData(false), use_tcp(use_tcp),
             wallConfig(NULL), quit_state(false), sock(0)
        {
            establishConnection();
            std::cout << "connection establish" << std::endl;
            receiveWallConfig(wallinfo);
            assert(wallConfig);
            
            if(me.rank == 0){
                wallConfig -> print();
            }

            me.barrier();
        }

        Client::~Client(){
            me.barrier();
        }

        void Client::establishConnection()
        {
            //Use MPI
            if(!use_tcp){
                if (me.rank == 0) {
                    if (hostName.empty()){
                        throw std::runtime_error("no mpi port name provided to establish connection");
                    }
                    std::cout << "#osp.dw: trying to connect to MPI port '"<< hostName << "'" << std::endl;
                }
                MPI_Comm newComm;
                MPI_CALL(Comm_connect(hostName.c_str() , MPI_INFO_NULL,0,me.comm,&newComm));
                // this is the port to communicate with server side
                displayGroup = mpicommon::Group(newComm);
                if (me.rank == 0){
                    std::cout << "connection established..." << std::endl;
                }
            }else{
                // Use TCP/IP
                // connect to server port receiving wall info
                std::cout << "=============================" << std::endl;
                std::cout << " Clients' workers are trying to connect to host " << hostName << 
                             " on port " << portNum << std::endl; 
                std::cout << std::endl;
                std::cout << "=============================" << std::endl;
                std::cout << std::endl;
                
                struct sockaddr_in serv_addr;
                // ~~ Socket Creation
                if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    printf("\n Socket creation error \n");
                }
  
                memset(&serv_addr, '0', sizeof(serv_addr));
  
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(portNum);
                //serv_addr.sin_addr.s_addr = inet_addr(hostName.c_str());
      
                // Convert IPv4 and IPv6 addresses from text to binary form
                if(inet_pton(AF_INET, "155.98.19.60", &serv_addr.sin_addr)<=0) 
                {
                    printf("\nInvalid address/ Address not supported \n");
                }
  
                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    printf("\nConnection Failed \n");
                }

            }
            me.barrier();
        }// end of establishConnection

        void Client::receiveWallConfig(const WallInfo *wallinfo)
        {
            //Use MPI
            if(!use_tcp){
                vec2i numDisplays;
                vec2i pixelsPerDisplay;
                int arrangement;
                int stereo;
                vec2f relativeBezelWidth;
                if (me.rank == 0) 
                    std::cout << "waiting for service to tell us the display wall config..." << std::endl;
                MPI_CALL(Bcast(&numDisplays,2,MPI_INT, 0, displayGroup.comm));
                std::cout << "numDisplay = " << numDisplays.x << std::endl;
                MPI_CALL(Bcast(&pixelsPerDisplay,2,MPI_INT, 0, displayGroup.comm));
                MPI_CALL(Bcast(&relativeBezelWidth,2,MPI_FLOAT, 0, displayGroup.comm));
                MPI_CALL(Bcast(&arrangement,1,MPI_INT, 0, displayGroup.comm));
                MPI_CALL(Bcast(&stereo,1,MPI_INT, 0, displayGroup.comm));
                wallConfig = new WallConfig(numDisplays,
                                                pixelsPerDisplay,
                                                relativeBezelWidth,
                                                (WallConfig::DisplayArrangement)arrangement,
                                                stereo);
            }else{
                // Use TCP/IP
                // Send client number first
                int temp;
                //if(me.rank == 1){
                    //write(receiver, me.size);
                    //flush(receiver);
                    //temp = read_int(receiver);
                //}
                // std::cout << "========== Receiving Wall Info ========== " << std::endl;
                // std::cout << "wallInfo: " << wallinfo ->numDisplays.x << " " << wallinfo ->numDisplays.y << std::endl;
                // std::cout << "wallInfo: " << wallinfo ->pixelsPerDisplay.x << " " << wallinfo ->pixelsPerDisplay.y << std::endl;
                hasMetaData = true;
                if(hasMetaData){
                    wallConfig = new WallConfig(wallinfo ->numDisplays,
                                                                    wallinfo ->pixelsPerDisplay,
                                                                    wallinfo ->relativeBezelWidth,
                                                                    (WallConfig::DisplayArrangement)wallinfo ->arrangement,
                                                                     wallinfo ->stereo);
                }
                me.barrier();
            }
        }// end of receiveWallConfig
        
        vec2i Client::totalPixelsInWall() const {
            assert(wallConfig);
            return wallConfig->totalPixels();
        }

        void Client::endFrame(){
            if(!use_tcp){
                MPI_CALL(Barrier(displayGroup.comm));
            }else{
                MPI_CALL(Barrier(me.comm));
            }
        }

        __thread void *g_compressor = NULL;
        
        void Client::writeTile(const PlainTile &tile)
        {
            assert(wallConfig);
            CompressedTile encoded;
            if (!g_compressor) g_compressor = CompressedTile::createCompressor();
            void *compressor = g_compressor;
            encoded.encode(compressor,tile);

            // Use MPI
            // if(!use_tcp){
            //     const box2i affectedDisplays = wallConfig->affectedDisplays(tile.region);
            //     for (int dy = affectedDisplays.lower.y; dy < affectedDisplays.upper.y; dy++){
            //         for (int dx = affectedDisplays.lower.x; dx< affectedDisplays.upper.x; dx++){
            //             encoded.sendTo(displayGroup, wallConfig->rankOfDisplay(vec2i(dx,dy)));
            //         }
            //     }
            // }else{
                static std::atomic<int> ID;
                int myTileID = ID++;
                // std::cout << "tile ID = " << ID << std::endl;
                // !! Send tile
                int out = 0;

                std::lock_guard<std::mutex> lock(sendMutex);
                
                // send how large the compressed data
                int compressedData = send(sock, &encoded.numBytes, sizeof(int), MSG_MORE);
                std::cout << "Compressed data size = " << encoded.numBytes << " bytes and send " << compressedData << std::endl;
                // send compressed tile
                out = send(sock, encoded.data, encoded.numBytes, 0);
                std::cout << " Send tile ID = " << myTileID << " contains " << encoded.numBytes << " bytes and send " << out << " bytes" << std::endl;

            //     // ## Debug 
            // CompressedTileHeader *header = (CompressedTileHeader *)encoded.data;

            // std::string filename = "Message" + std::to_string(header ->region.lower.x) + "_"
            //                                  + std::to_string(header ->region.lower.y) + "_"
            //                                  + std::to_string(header ->region.upper.x) + "_"
            //                                  + std::to_string(header ->region.upper.y) + ".ppm";
            // ospcommon::vec2i img_size(256, 256);
            // writePPM(filename.c_str(), &img_size, (uint32_t*)header -> payload);
            //     // Correct !

            //     const box2i affectedDisplays = wallConfig -> affectedDisplays(tile.region);

            //     //outbox.push_back(message);
            //     //std::cout << "debug 1" << std::endl;
            //     printf("region %i %i - %i %i displays %i %i - %i %i\n",
            //               tile.region.lower.x,
            //               tile.region.lower.y,
            //               tile.region.upper.x,
            //               tile.region.upper.y,
            //               affectedDisplays.lower.x,
            //               affectedDisplays.lower.y,
            //               affectedDisplays.upper.x,
            //               affectedDisplays.upper.y);
            //     //// first save the number of rank that need to send to
            //     // then save rank number;
            //     // then save tile data

            // }
        }// end of writeTile

        void Client::sendTile()
        {
            while(true){
            //std::cout << "outbox is " << outbox.empty() << std::endl;
                //if(!outbox.empty()){
                    //std::cout << "debug 1" << std::endl;
                    int count = 0;
                    //std::lock_guard<std::mutex> lock(state_mutex);
                    auto outgoingTiles = outbox.consume();
                    for(auto &msg : outgoingTiles){
                        //std::cout << "count = " << count << std::endl;
                        count++;
                        // zmq::message_t tilemsg(msg ->size);
                        // unsigned char *data = (unsigned char *)tilemsg.data();
                        //std::cout << "message size " << msg ->size << std::endl;
                        //memcpy(data, &numOfRank, sizeof(int));
                         //memcpy(&data[4], toRank.data(), sizeof(int) * numOfRank);
                        // memcpy(data, msg ->data, msg -> size);
                        //std::lock_guard<std::mutex> lock(sendMutex);
                        //static double start = getSysTime();
                        // int send = socket.send(tilemsg, 0);
                        //std::cout << "Client rank " << me.rank << " send " << send << std::endl;
                            
                        // int recv = socket.recv(&receive1);
                        //static double end = getSysTime();
                        //std::cout << "Send tile time = " << end - start << std::endl;
                        //std::cout << "Client rank " << me.rank << " receive " << recv << std::endl;
                        //std::cout << "Rank " << me.rank << " Client: 1 : receive responce" << std::endl;
                    }
                //}
            }
            //if(quit_state){
                //return;
            //}
        }// end of send Tile


    }// ospray::dw
}// ospray
