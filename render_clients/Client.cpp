#include "Client.h"
#include <atomic>
#include <thread>


namespace ospray{
    namespace dw{
        
        Client::Client(const std::string &hostName,
                       const int &portNum,
                       const mpicommon::Group &me,
                       const WallInfo *wallinfo)
            :hostName(hostName), portNum(portNum),
             me(me), wallConfig(NULL), sock(0)
        {
            //! Connect to head node of display cluster  
            establishConnection();
            std::cout << "connection establish" << std::endl;

            //! Construct wall configurations
            constructWallConfig(wallinfo);
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
            // connect to the head node of display cluster
            std::cout << "=============================" << std::endl;
            std::cout << " Clients' worker #" << me.rank << 
                         " is trying to connect to host " << hostName <<  
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
     
            // TODO: connect use hostname instead of IP address  
            // Convert IPv4 and IPv6 addresses from text to binary form
            // if(inet_pton(AF_INET, "155.98.19.60", &serv_addr.sin_addr)<=0) 
            // {
            //     printf("\nInvalid address/ Address not supported \n");
            // }
            if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
            {
                printf("\nInvalid address/ Address not supported \n");
            }
  
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                printf("\nConnection Failed \n");
            }

            me.barrier();
        }// end of establishConnection

        void Client::constructWallConfig(const WallInfo *wallinfo)
        {
            wallConfig = new WallConfig(wallinfo ->numDisplays,
                                        wallinfo ->pixelsPerDisplay,
                                        wallinfo ->relativeBezelWidth,
                                        (WallConfig::DisplayArrangement)wallinfo ->arrangement,
                                        wallinfo ->stereo);
            me.barrier();
        }
        
        vec2i Client::totalPixelsInWall() const 
        {
            assert(wallConfig);
            return wallConfig->totalPixels();
        }

        void Client::endFrame()
        {
            MPI_CALL(Barrier(me.comm));
        }

        __thread void *g_compressor = NULL;
        
        void Client::writeTile(const PlainTile &tile)
        {
            //assert(wallConfig);
            
            CompressedTile encoded;
            if (!g_compressor) g_compressor = CompressedTile::createCompressor();
            void *compressor = g_compressor;
            encoded.encode(compressor,tile);

            static std::atomic<int> ID;
            int myTileID = ID++;
            //std::cout << "tile ID = " << ID << std::endl;
            // TODO: Measure compression ratio
            // TODO: Push all rendered tiles into a outbox and send them in a separate thread 
            // !! Send tile
            //! Send how large the compressed data
            // TODO: Measure sending time
            std::lock_guard<std::mutex> lock(sendMutex);
            int compressedData = send(sock, &encoded.numBytes, sizeof(int), MSG_MORE);
            //std::cout << "Compressed data size = " << encoded.numBytes << " bytes and send " << compressedData << std::endl;
            //! Send compressed tile
            int out = send(sock, encoded.data, encoded.numBytes, 0);
            //std::cout << " Send tile ID = " << myTileID << " contains " << encoded.numBytes << " bytes and send " << out << " bytes" << std::endl;

            // ## Debug 
            // CompressedTileHeader *header = (CompressedTileHeader *)encoded.data;

            // std::string filename = "Message" + std::to_string(header ->region.lower.x) + "_"
            //                                  + std::to_string(header ->region.lower.y) + "_"
            //                                  + std::to_string(header ->region.upper.x) + "_"
            //                                  + std::to_string(header ->region.upper.y) + ".ppm";
            // ospcommon::vec2i img_size(256, 256);
            // writePPM(filename.c_str(), &img_size, (uint32_t*)header -> payload);
            // Correct !

            //outbox.push_back(message);
            //printf("region %i %i - %i %i displays %i %i - %i %i\n",
                    //tile.region.lower.x,
                    //tile.region.lower.y,
                   //tile.region.upper.x,
                   //tile.region.upper.y,
                   //affectedDisplays.lower.x,
                   //affectedDisplays.lower.y,
                   //affectedDisplays.upper.x,
                   //affectedDisplays.upper.y);

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
