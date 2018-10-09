#include "ServiceInfo.h"


using namespace ospcommon;

namespace ospray{
    namespace dw{
        
        /*! read a service info from a given hostName:port. The service
            has to already be running on that port */
        void ServiceInfo::getFrom(const std::string &hostName,
                                  const int portNo)
        {
            int sock = 0, valread;
            struct sockaddr_in serv_addr;
            // ~~ Socket Creation
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                printf("\n Socket creation error \n");
            }
            memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portNo);
            //serv_addr.sin_addr.s_addr = inet_addr(hostName.c_str());

            //Convert IPv4 and IPv6 addresses from text to binary form
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

            // !! MPI Port Name
            int string_length = 0;
            valread = recv(sock, &string_length, 4, 0);
            // std::cout << " Value read = " << valread << " The length of string is " << string_length << std::endl;
            char buffer[1024] = {0};
            valread = recv(sock, buffer, string_length, 0);
            // std::cout << "Received port name = " << buffer << std::endl;
            mpiPortName = buffer;
            // wallInfo
            char *data = new char [sizeof(WallInfo)];
            valread = recv(sock, data, sizeof(WallInfo), 0);
            wallInfo = (WallInfo*) data;
            // std::cout << "wall Info: " << std::endl;
            // std::cout << " num of displays = " << wallInfo ->numDisplays.x << " " 
            //                                                         << wallInfo ->numDisplays.y << std::endl;
            // std::cout << " pixel per display = " << wallInfo ->pixelsPerDisplay.x << " "
            //                                                         << wallInfo -> pixelsPerDisplay.y << std::endl;
            // std::cout << " total pixels in the wall = " << wallInfo ->totalPixelsInWall.x << " "
            //                                                                  << wallInfo -> totalPixelsInWall.y << std::endl;

            // !! Send the number of clients
            // int clientNum = 5;
            // send(sock, &clientNum, 4, 0);
            send(sock, &mpicommon::worker.size, 4, 0);
           //std::cout << "there are " << mpicommon::worker.size << " clients " << std::endl;
           //write(sock, mpicommon::worker.size);
           //flush(sock);
           close(sock);
        } // ! End of getFrom

        void ServiceInfo::sendTo(WallInfo &wallInfo, const std::string &hostName, int &clientNum){
            int info_portNum = 8443;
            int server_fd, valread;
            int new_socket;
            int opt = 1;
            struct sockaddr_in address;
            int addrlen = sizeof(address);
            // Creating socket file descriptor
            if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
                perror("socket failed");
                exit(EXIT_FAILURE);
            }
            // Forcefully attaching socket to the port 8080
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
            {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(info_portNum);

            // Forcefully attaching socket to the port 8080
            if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
            {
                perror("bind failed");
                exit(EXIT_FAILURE);
            }
            if (listen(server_fd, 3) < 0)
            {
                perror("listen");
                exit(EXIT_FAILURE);
            }
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // send mpi port name
            int str_length = hostName.length();
            std::cout << "string length = " << str_length << std::endl;
            send(new_socket, &str_length, 4, 0);
            send(new_socket, hostName.data(), str_length, 0);
            // Send wall Info
            send(new_socket, &wallInfo, sizeof(WallInfo), 0);
            // Read the number of clients will connect 
            valread = recv(new_socket, &clientNum, 4, 0);
            std::cout << " DEBUG: clientNum = " << clientNum << std::endl;
        }// ! End of sendTo
}// end of ospray::dw
}// end of ospray
