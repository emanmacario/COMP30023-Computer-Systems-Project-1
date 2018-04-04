#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define SERVER_PORT 12345
#define BUF_SIZE 4096
#define QUEUE_SIZE 10


int main(int argc, char *argv[]) {

    int sockfd, connfd;


    // buffer for outgoing files
    char buffer[BUF_SIZE];

    // holds IP address
    struct sockaddr_in serv_addr;

    
    memset(&serv_addr, '0', sizeof(serv_addr));  // initialise server address
    memset(buffer, '0', sizeof(buffer)); // initialise send buffer


    // build address structure to bind to socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address

    // Note: sin_addr.s_addr is just a 32 bit int (4 bytes)

    serv_addr.sin_port = htons(SERVER_PORT);  // listen on the default port specified


    // create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }


    // Bind the socket to the server address
    bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));


    // Listen, which allocates space to queue incoming calls for the
    // case that several clients try to connect at the same time. (non-blocking call)
    listen(sockfd, QUEUE_SIZE);


    // Socket is now set up and bound. Wait for connection and process it.
    connfd = accept(sockfd, (struct sockaddr*)NULL, NULL); // block for connection request

    


    return 0;
}