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
#define CONTENT_TYPE_LEN 22


typedef struct {
    char *filename;
    char *response_code;
} http_response_t;


// Function prototypes
char *get_content_type(char *filename);
void usage(char *prog_name);





/* Gives user information on how
 * to use the program with corresponding
 * command line arguments.
 */
void usage(char *prog_name) {
    printf("Usage: %s [port number] [path to web root]\n", prog_name);
    exit(EXIT_SUCCESS);
}



// Reads in a request from the file descriptor associated with
// a connection
char *get_request_line(int newfd) {

    FILE *fdstream = fdopen(newfd, "r");

    if (fdstream == NULL) {
        perror("Error opening file descriptor");
        exit(EXIT_FAILURE);
    }

    ssize_t read;
    size_t len = 0;
    char *request_line = NULL;

    // Read the request line
    if ((read = getline(&request_line, &len, fdstream)) == -1) {
        perror("Error reading HTTP request");
        exit(EXIT_FAILURE);
    }


    /* MIGHT NEED THIS IF NEED TO FLUSH THE STREAM
    while ((read = getline(&request_line, &len, fdstream)) != -1) {
        printf("Retrieved line of length: %zu\n", read);
        printf("Line: %s\n", request_line);
    }
    */

    fflush(fdstream);
    fclose(fdstream);

    // NEED TO FREE THE ALLOCATED BUFFER LATER
    return request_line;
}


/* Parse the HTTP request line to get the filename.
 */
char *get_filename(char *request_line) {

    char *filename = malloc(sizeof(char)*strlen(request_line));
    assert(filename);

    // Get the filename from the request line
    if (sscanf(request_line, "GET %s HTTP/1.0\n", filename) != 1) {

        // MIGHT NOT NEED THIS LINE
        sscanf(request_line, "GET %s HTTP/1.1\n", filename);
    }

    return filename;
}



// Gets the path to the file requested. Can either be an absolute
// path or relative path, depending on the web root specified in
// command line.
char *get_path_to_file(char *path_to_web_root, char *filename) {

    // Calculate total length of path to the requested file
    size_t path_len = strlen(path_to_web_root) + strlen(filename) + 1;

    // Allocate space for the path name
    char *path;
    if ((path = malloc(sizeof(char) * path_len)) == NULL) {
        perror("Error allocating memory for path name");
        exit(EXIT_FAILURE);
    }
    
    // Create the path name from the web root and requested filename
    strcpy(path, path_to_web_root);
    strcat(path, filename);

    // Free memory allocated to filename, since we don't need it anymore
    free(filename);

    return path;
}




/* Parses the filename the client wants, and
 * returns the MIME type (content type) associated
 * with the file's extension.
 */
char *get_content_type(char *filename) {

    // The max length content type i.e. application/javascript
    const size_t max_content_type_len = 22;

    char *content_type = malloc(sizeof(char)*(max_content_type_len+1));
    if (content_type == NULL) {
        perror("Failed to allocate memory to content type");
        exit(EXIT_FAILURE);
    }

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
    return content_type;
}



void handle_request(int newfd) {

    // Process a request (just print info about the shit)
    char *request_line = get_request_line(newfd);
    char *filename     = get_filename(request_line);
    char *content_type = get_content_type(filename);
    char *path_to_file = get_path_to_file(path_to_web_root, filename);


    // Open the file and send its contents to client
    FILE *fp;
    if ((fp = fopen(path_to_file, "r")) == NULL) {
        // TO DO: Send 404 response
    } else {
        printf("Sending file...\n");
        while (fgets(buffer, BUF_SIZE, fp) != NULL) {
            send(newfd, buffer, strlen(buffer), 0);
            printf("%s", buffer);
        }
        fclose(fp);
    }


    // Free allocated memory
    free(request_line);
    free(filename);
    free(content_type);
    free(path_to_file);
}


/* Opens up a file and returns a string 
 * containing the contents of the file.
 */
char *get_body(FILE *fp) {
    // Sanity check
    assert(fp);

    // Initialise memory for body
    size_t size = 1;
    char *body = malloc(sizeof(char)*size);
    if (body == NULL) {
        perror("Error allocating memory to HTTP response body");
        exit(EXIT_FAILURE);
    }
    body[0] = '\0';

    ssize_t read;
    size_t len = 0;
    char *line = NULL;
    while ((read = getline(&line, &len, fp)) != -1) {
        // allocate extra space for line just read
        body = realloc(body, size+len);
        // update the size of the body
        size += len;
        // append the line to the body
        strcat(body, line);
        // Need to free and reset pointer
        free(line);
        line = NULL;
    }
    free(line);
    line = NULL;

    // Close the file here
    fclose(fp);

    return body;
}



void response_404(int newfd) {

    send 

}



void response_200(int newfd) {

}


void send_response(int newfd, char *response) {

}


/****************************************************************************/
// FUNCTIONS UNSURE OF

void serve(int sockfd, char *filename) {
    int newfd;
    FILE *fp;
    char buffer[BUF_SIZE];

    // Forever
    for (;;) {

        // Block until a new connection is established
        if ((newfd = accept(sockfd, (struct sockaddr*)NULL, NULL)) < 0) {
            perror("Error on accept\n");
            exit(EXIT_FAILURE);
        }

        // Open the file and send its contents to client
        if ((fp = fopen(filename, "r")) == NULL) {
            perror("Error opening file\n");
            exit(EXIT_FAILURE);
        } else {
            while (fgets(buffer, BUF_SIZE, fp) != NULL) {
                send(newfd, buffer, strlen(buffer), 0);
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

/*************************************************************************/



/** MAIN FUNCTION **/
int main(int argc, char *argv[]) {

    // check if command line arguments supplied
    if (argc < NUM_PARAMS) {
        usage(argv[0]);
    } 

    // get the port number and path to web root
    char *port_num = argv[1];
    char *path_to_web_root = argv[2];


    int sockfd, newfd;

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
        perror("Error getting address info");
        fprintf(stderr, "%s\n", gai_strerror(status));
    }


    // res now points to a linked list of 1 or more struct addrinfos


    // create a socket
    if ((sockfd = socket(res->ai_family, res->ai_socktype, 0)) < 0) {
        perror("Error opening socket");
        exit(EXIT_FAILURE);
    }

    printf("Successfully created socket\n");



    // set socket options to allow reuse of a port if server closes
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("Error setting socket options");
        exit(EXIT_FAILURE);
    }


    printf("Successfully set socket options\n");



    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    printf("Successfully bound socket\n");


    // Listen, which allocates space to queue incoming calls for the
    // case that several clients try to connect at the same time. (non-blocking call)
    if (listen(sockfd, BACKLOG) < 0) {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }


    printf("Successfully listened on socket\n");

    struct sockaddr_storage client;
    socklen_t client_len = sizeof(client);


    // Socket is now set up and bound. Wait for connection and process it.
    // Use the accept function to retrieve a connect request and convert it
    // into a connection.


    // Main loop of server
    for (;;) {

        // Block until a connection request arrives
        newfd = accept(sockfd, (struct sockaddr*)&client, &client_len);

        // Check if connection was successfull
        if (newfd < 0) {
            perror("ERROR on accept");
            exit(0);
        }

        printf("Successfully accepted a client connection request\n");


        handle_request(newfd);
        
        


        
        printf("Succesfully sent requested file\n");

        // Close the connection to the client
        close(newfd);
    }

    

    // If we don't care about a client's identity, can set client address and
    // length parameters (last 2 parameters to accept) as NULL. Otherwise,
    // before calling accept, we need to set addr parameter to a buffer large
    // enough to hold the address, and set the integer pointed to by len to
    // the size of the buffer in bytes.

    
    /* close socket */
    close(sockfd);

    /* free the linked list of results for IP addresses of host */
    freeaddrinfo(res);

    return 0;
}



