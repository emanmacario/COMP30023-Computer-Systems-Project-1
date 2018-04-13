#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>   // needed for socket function
#include <netinet/in.h>   // needed for internet address i.e. sockaddr_in and in_addr
#include <netdb.h>


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>  // needed for byte order functions
#include <unistd.h>     // needed for read/close functions


#include <assert.h>


#define BUF_SIZE 8192  // buffer size
#define BACKLOG 10     // total pending connections queue will hold
#define NUM_PARAMS 3   // number of command line arguments


// Function prototypes
void get_content_type(char *filename, char *content_type);
void usage(char *prog_name);


int main(int argc, char *argv[]) {

    // check if command line arguments supplied
    if (argc < NUM_PARAMS) {
        usage(argv[0]);
    } 

    // get the port number and path to web root
    char *port_num = argv[1];
    char *path_to_web_root = argv[2];

    //printf("Port number: %d\nPath to web root: %s\n", port_num, path_to_web_root);


    int sockfd, connfd;

    // buffer for outgoing files
    char buffer[BUF_SIZE];

    


    int status;
    struct addrinfo hints, *res; // point to results

    memset(&hints, 0, sizeof(hints)); // make sure struct is empty
    hints.ai_family = AF_INET;        // use IPv4 address
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;      // fill in my IP for me



    status = getaddrinfo(NULL, port_num, &hints, &res);
    if (status != 0) {
        perror("Error getting address info\n");
        fprintf(stderr, "%s\n", gai_strerror(status));
    }


    // res now points to a linked list of 1 or more struct addrinfos


    // create a socket
    if ((sockfd = socket(res->ai_family, res->ai_socktype, 0)) < 0) {
        perror("Error opening socket\n");
        exit(EXIT_FAILURE);
    }



    // set socket options to allow reuse of a port if server closes
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("Error setting socket options\n");
        exit(EXIT_FAILURE);
    }




    // Bind the socket to the server address
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Error binding socket\n");
        exit(EXIT_FAILURE);
    }





    // Listen, which allocates space to queue incoming calls for the
    // case that several clients try to connect at the same time. (non-blocking call)
    if (listen(sockfd, BACKLOG) < 0) {
        perror("Error listening on socket\n");
        exit(EXIT_FAILURE);
    }


    struct sockaddr_storage client;
    socklen_t client_len = sizeof(client);


    // Socket is now set up and bound. Wait for connection and process it.
    // Use the accept function to retrieve a connect request and convert it
    // into a connection.


    // Main loop of server
    int n;
    for (;;) {

        // Block until a connection request arrives
        connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);

        // Check if connection was successfull
        if (connfd < 0) {
            perror("ERROR on accept");
            exit(0);
        }

        // Send a message
        char *msg = "eman is the sickest";
        send(connfd, msg, strlen(msg), 0);
        

        // Read from a sokcet
        n = read(connfd, buffer, BUF_SIZE-1);
        
        if (n < 0) {
            perror("ERROR reading from socket");
            exit(0);
        }




        printf("%s",buffer);        
    }

    

    // Note: accept will block until a connect request arrives.
    // It can be modified to be non-blocking.


    // If we don't care about a client's identity, can set client address and
    // length parameters (last 2 parameters to accept) as NULL. Otherwise,
    // before calling accept, we need to set addr parameter to a buffer large
    // enough to hold the address, and set the integer pointed to by len to
    // the size of the buffer in bytes.


    // Note: the file descriptor returned by accept is a socket descriptor
    // that is connected to the client that called connect. This new socket
    // descriptor, e.g. connfd has the same socket type and address family as
    // the original socket, sockfd.


    
    /* close socket */
    close(sockfd);



    freeaddrinfo(res);


    return 0;
}






/* Gives user information on how
 * to use the program with corresponding
 * command line arguments.
 */
void usage(char *prog_name) {
    printf("Usage: %s [port number] [path to web root]\n", prog_name);
    exit(EXIT_SUCCESS);
}



/* Parse the HTTP request line to get the filename.
 */
char *get_filename(char *request_line) {

    char *filename = malloc(strlen(request_line));
    assert(filename);

    // Get the filename from the request line
    sscanf(request_line, "GET %s HTTP/1.0\r\n", filename);

    return filename;
}



/* Parses the filename the client wants, and
 * returns the MIME type (content type) associated
 * with the file's extension.
 */
void get_content_type(char *filename, char *content_type) {

    // Get the content type associated with the file extension.
    if (strstr(filename, ".html") != NULL) {
        strcpy(content_type, "text/html");
    } else if (strstr(filename, ".jpg") != NULL) {
        strcpy(content_type, "image/jpeg");
    } else if (strstr(filename, ".css") != NULL) {
        strcpy(content_type, "text/css");
    } else if (strstr(filename, ".js") != NULL) {
        strcpy(content_type, "application/javascript");
    }
}



void serve(int sockfd, char *filename) {
    int connfd;
    FILE *fp;
    char buffer[BUF_SIZE];

    // Forever
    for (;;) {

        // Block until a new connection is established
        if ((connfd = accept(sockfd, (struct sockaddr*)NULL, NULL)) < 0) {
            perror("Error on accept\n");
            exit(EXIT_FAILURE);
        }

        // Open the file and send its contents to client
        if ((fp = fopen(filename, "r")) == NULL) {
            perror("Error opening file\n");
            exit(EXIT_FAILURE);
        } else {
            while (fgets(buffer, BUF_SIZE, fp) != NULL) {
                send(connfd, buffer, strlen(buffer), 0);
            }
            fclose(fp);
        }
    }
}



/*************************************************************************/
// GETTING IP ADDRESS ASSOCIATED WITH SOCKET
void get_ip_address(int sockfd) {

    // Note: if IP address is 0.0.0.0, this means server is configured
    // to listen on all IPv4 addresses of the local machine.

    struct sockaddr_in check_addr;
    memset(&check_addr, '0', sizeof(check_addr));

    socklen_t alenp = sizeof(check_addr);
    int a = getsockname(sockfd, (struct sockaddr*)&check_addr, &alenp);

    char ip_address[16];
    inet_ntop(AF_INET, &check_addr.sin_addr, ip_address, sizeof(ip_address));

    printf("a = %d\n", a);
    printf("IPv4 address: %s\n", ip_address);
}