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
            // send rank to service 
            sendRank();
            //! Construct wall configurations
            constructWallConfig(wallinfo);
            assert(wallConfig); 
            
            numPixelsPerClient = wallConfig ->calculateNumPixelsPerClient(me.size);
            numWrittenThisClient = std::vector<int>(me.size, 0);
            //numExpectedThisFrame = wallConfig ->totalPixels().product();    
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
                
            struct sockaddr_in serv_addr;
            // ~~ Socket Creation
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                printf("\n Socket creation error \n");
            }
  
            memset(&serv_addr, '0', sizeof(serv_addr));
  
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portNum);
            serv_addr.sin_addr.s_addr = inet_addr(hostName.c_str());
            //Convert IPv4 and IPv6 addresses from text to binary form
            // "155.98.19.60" powerwall00
            hostent *record = gethostbyname(hostName.c_str());
            if(record == NULL){
		        printf("%s is unavailable\n", hostName);
		        exit(1);
	        }
            in_addr * address = (in_addr * )record->h_addr;
	        std::string ip_address = inet_ntoa(* address);
            // std::cout << hostName << " (" << ip_address << ")\n";
            if(inet_pton(AF_INET, ip_address.c_str(), &serv_addr.sin_addr)<=0) 
            {
                printf("\nInvalid address/ Address not supported \n");
            } 

            while(1){
                int connection = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                if (connection >= 0)
                {
                    printf("\nConnection establish \n");
                    break;
                }
            }
            me.barrier();
        }// end of establishConnection

        void Client::sendRank(){
            int rank = me.rank;
            int s = send(sock, &rank, sizeof(int), 0);
            me.barrier();
        }

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

        void Client::printStatistics(){
            // Print Statistics Here
            if(!sendTime.empty()){
                Stats sendStats(sendTime);
                sendStats.time_suffix = "ms";
                std::cout  << "Message send statistics:\n" << sendStats << "\n";
            }
            if(!compressiontimes.empty()){
                Stats compressionStats(compressiontimes);
                compressionStats.time_suffix = "ms";
                std::cout  << "Compression time statistics:\n" << compressionStats << "\n";
            }
        }

        void Client::endFrame()
        {
            if(me.rank == 0){
                // printStatistics();
            }
            MPI_CALL(Barrier(me.comm));
        }

        __thread void *g_compressor = NULL;
        
        void Client::writeTile(const PlainTile &tile)
        {
            CompressedTile encoded;
            if (!g_compressor) g_compressor = CompressedTile::createCompressor();
            void *compressor = g_compressor;
            // auto start0 = std::chrono::high_resolution_clock::now();
            encoded.encode(compressor,tile);
            // auto end0 = std::chrono::high_resolution_clock::now();
            // compressiontimes.push_back(std::chrono::duration_cast<realTime>(end0 - start0));

            // TODO: Measure compression ratio
            // TODO: Push all rendered tiles into a outbox and send them in a separate thread 
            // !! Send tile
            //! Send how large the compressed data
            {
                std::lock_guard<std::mutex> lock(sendMutex);
                // auto start = std::chrono::high_resolution_clock::now();
                // printf("Rank # %i region %i %i - %i %i \n", 
                //                                                     me.rank,
                //                                                     region.lower.x,
                //                                                     region.lower.y,
                //                                                     region.upper.x,
                //                                                     region.upper.y);
                int compressedData = send(sock, &encoded.numBytes, sizeof(int), MSG_MORE);
                // std::cout << "Compressed data size = " << encoded.numBytes << " bytes and send " << compressedData << std::endl;
                //! Send compressed tile
                int out = send(sock, encoded.data, encoded.numBytes, 0);
                // auto end = std::chrono::high_resolution_clock::now();
                // sendtimes.push_back(std::chrono::duration_cast<realTime>(end - start));
            }
            // ## Debug 
            // CompressedTileHeader *header = (CompressedTileHeader *)encoded.data;

            // std::string filename = "Message" + std::to_string(header ->region.lower.x) + "_"
            //                                  + std::to_string(header ->region.lower.y) + "_"
            //                                  + std::to_string(header ->region.upper.x) + "_"
            //                                  + std::to_string(header ->region.upper.y) + ".ppm";
            // ospcommon::vec2i img_size(256, 256);
            // writePPM(filename.c_str(), &img_size, (uint32_t*)header -> payload);
            // Correct !
        }// end of writeTile

    }// ospray::dw
}// ospray
