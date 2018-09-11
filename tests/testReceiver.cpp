#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "mpiCommon/MPICommon.h"
#include "ospray/ospray_cpp/Device.h"

using namespace ospcommon;


#define PORT 8080
  
extern "C" int main(int argc, char const *argv[])
{
     if(ospInit(&argc, argv) != OSP_NO_ERROR){
        throw std::runtime_error("Cannot Initialize OSPRay");
    }else{
        std::cout << "Initialize OSPRay Successfully" << std::endl;
    } 
   
    mpicommon::init(&argc, argv, true);
    mpicommon::Group world(MPI_COMM_WORLD);
    std::cout << " Initialize MPI World! There are " << world.size << " clients" << std::endl; 
       
    int sock = 0, valread;
    
    struct sockaddr_in serv_addr;
    
    char *hello = "Hello from client";
    char buffer[1024] = {0};
    // ~~ Socket Creation
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
      
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
  
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    send(sock , hello , strlen(hello) , 0 );
    printf("Hello message sent\n");
    valread = read( sock , buffer, 1024);
    printf("%s\n",buffer );
    return 0;
}

/*
#include <iostream>
#include <vector>

#include "ospcommon/networking/Socket.h"
#include "ospcommon/containers/TransactionalBuffer.h"
#include "mpiCommon/MPICommon.h"
#include "ospray/ospray_cpp/Device.h"

using namespace ospcommon;

extern "C" int main(int argc, const char **argv){
    
    if(ospInit(&argc, argv) != OSP_NO_ERROR){
        throw std::runtime_error("Cannot Initialize OSPRay");
    }else{
        std::cout << "Initialize OSPRay Successfully" << std::endl;
    } 
   
    mpicommon::init(&argc, argv, true);
    mpicommon::Group world(MPI_COMM_WORLD);
    std::cout << " Initialize MPI World! There are " << world.size << " clients" << std::endl; 
    
    std::string hostName = "han";
    int portNum = 2903;
    
    //! Connect to server socket on port  
    socket_t receiver = connect(hostName.c_str(), portNum);
    
    if(receiver != NULL){
        std::cout << "Connect to the server: host is " << hostName << " on port " << portNum << std::endl;
    }

    // Keep Sending
    int size = 100;
    std::vector<int> vec_test(size, 0);
    std::cout << "The second data here is " << vec_test[1] << std::endl;
    while(true){
        if(receiver != NULL){
            std::cout << "writing" << std::endl;
            write(receiver, vec_test.data(), vec_test.size() * sizeof(int));
            flush(receiver);
        }else{
            close(receiver);
        }
    }
   
    return 0;
    
}
*/
