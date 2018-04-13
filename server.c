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


#define BUF_SIZE 8192
#define QUEUE_SIZE 10
#define NUM_PARAMS 3


// Function prototypes
void get_content_type(char *filename, char *content_type);
void usage(char *prog_name);


int main(int argc, char *argv[]) {

    // check if command line arguments supplied
    if (argc < NUM_PARAMS) {
        usage(argv[0]);
    } 

    // get the port number and path to web root
    uint16_t port_num = atoi(argv[1]);
    char *path_to_web_root = argv[2];

    //printf("Port number: %d\nPath to web root: %s\n", port_num, path_to_web_root);


    int sockfd, connfd;

    // buffer for outgoing files
    char buffer[BUF_SIZE];

    // holds IP address of server
    struct sockaddr_in serv_addr, cli_addr; // Note: cli_addr will be filled in with address of the peer socket.
    socklen_t clilen;

    
    memset(&serv_addr, '0', sizeof(serv_addr));  // initialise server address
    memset(buffer, '0', sizeof(buffer)); // initialise send buffer


    // build address structure to bind to socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address (socket endpoint will be bound to all the system's network interfaces)

    // Note: sin_addr.s_addr is just a 32 bit unsigned int (i.e. uint32_t)

    serv_addr.sin_port = htons(port_num);  // listen on the port specified in cmd line


    // create a socket
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error opening socket\n");
        exit(EXIT_FAILURE);
    }


    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error binding socket\n");
        exit(EXIT_FAILURE);
    }





    // Listen, which allocates space to queue incoming calls for the
    // case that several clients try to connect at the same time. (non-blocking call)
    listen(sockfd, QUEUE_SIZE);


    // Must initialise to contain the size (in bytes) of the structure pointed
    // to by &cli_addr. See 'man accept' for more details.
    clilen = sizeof(cli_addr);

    // Socket is now set up and bound. Wait for connection and process it.
    // Use the accept function to retrieve a connect request and convert it
    // into a connection.


    // Main loop of server
    int n;
    while (1) {

        // Block until a connection request arrives
        connfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check if connection was successfull
        if (connfd < 0) {
            perror("ERROR on accept");
            exit(0);
        }

        // Read from a sokcet
        n = read(sockfd, buffer, BUF_SIZE-1);
        
        if (n < 0) {
            perror("ERROR reading from socket");
            exit(0);
        }

        printf("%s\n",buffer);        
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
void get_filename(char *request_line) {

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