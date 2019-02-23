#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 8080

extern "C" int main(int argc, char const *argv[])
{
    int server_fd, new_socket, valread, sd;
    struct sockaddr_in address;

    int opt = 1;
    int addrlen = sizeof(address);

    int max_clients = 30;
    int activity;
    int client_socket[30];
    char buffer[1024] = {0};
    char *hello = "Hello from server";
    int max_sd; 
    // Set of socket descriptors
    fd_set readfds;

    //initialise all client_socket[] to 0 so not checked
    for (int i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    // Creating master socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
      
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
      
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, 
                                 sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // Accept incoming connections
    printf("Waiting for connections ... ");

    while(true){
        //clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

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

        // wait for an activity on one of the sockets, time out is NULL,
        // so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if((activity < 0) && (errno != EINTR)){
            printf("select error");
        }
        
        //If something happened on the master socket , 
        //then its an incoming connection 
        if (FD_ISSET(server_fd, &readfds))  
        {  
            if ((new_socket = accept(server_fd, 
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
            
            //inform user of socket number - used in send and receive commands 
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs
                  (address.sin_port));  
          
            //send new connection greeting message 
            if( send(new_socket, hello, strlen(hello), 0) != strlen(hello) )  
            {  
                perror("send");  
            }  
                
            puts("Welcome message sent successfully");  
                
            //add new socket to array of sockets 
            for (int i = 0; i < max_clients; i++)  
            {  
                //if position is empty 
                if( client_socket[i] == 0 )  
                {  
                    client_socket[i] = new_socket;  
                    printf("Adding to list of sockets as %d\n" , i);  
                        
                    break;  
                }  
            }  
        }
        
        //else its some IO operation on some other socket
        for (int i = 0; i < max_clients; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds))
            {
                //Check if it was for closing , and also read the
                //incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , \
                        (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" ,
                          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }

                //Echo back the message that came in
                else
                {
                    //set the string terminating NULL byte on the end
                    //of the data read
                    buffer[valread] = '\0';
                    send(sd , buffer , strlen(buffer) , 0 );
                }
            }
        }
    }

    return 0;
}



/*
 //Here is the sender side 
 //need to send wall info and receive the number of clients will connect

#include <iostream>
#include <vector>
#include "ospcommon/networking/Socket.h"

using namespace ospcommon;

extern "C" int main(int argc, char **argv){
    std::string hostName = "han";
    int portNum = 2903;
   
    Socket Creation
   int sockfd = socket( 
    // bind to port 
    socket_t sender = NULL;
    sender = bind(portNum);
    if(sender != NULL){
        std::cout << "Server opened a port on " << portNum << std::endl;
    }else{
        std::cout << "Failed! Server cannot open the port" << std::endl;
    }
    
    // set of socket descriptors
    fd_set readfds; 

    // ! Got clients connected
    std::vector<socket_t> client_connections;

    for(int i = 0; i < 2; i++){
        socket_t client = listen(sender);
        std::cout << "Got clent connection ... " << std::endl;
        client_connections.push_back(client);
    }

    int max_sd;
    // Receive from clients
    while(true){
    //     clear the socket set
    //    FD_SET(sender, &readfds);
    //    max_sd = sender;
        for(int i = 0; i < client_connections.size(); i++){
            int numBytes = 0;
            int max_bytes = 400;
            int *data_received = new int[max_bytes];
            read(client_connections[i], data_received, max_bytes/ 4);
            int *data = (int *) data_received;
            std::cout << "the second number of data received is " << data[1] << std::endl; 
        }
    }
   
    return 0;
}
*/
