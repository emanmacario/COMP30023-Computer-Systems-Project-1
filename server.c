#include <sys/types.h>
#include <sys/fcntl.h>    // need for open O_ constants
#include <sys/socket.h>   // needed for socket function
#include <netinet/in.h>   // needed for internet address i.e. sockaddr_in and in_addr
#include <netdb.h>        // needed for setting up server i.e. AI_PASSIVE and shit

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>  // needed for byte order functions
#include <unistd.h>     // needed for read/close functions


#include <pthread.h>    // for POSIX threads

#define BUFFER_SIZE 8192  // buffer size
#define BACKLOG 10     // total pending connections queue will hold



// Function prototypes
char *get_content_type(char *filename);
void usage(char *prog_name);
char *get_request_line(FILE *fdstream);
char *get_filename(char *request_line);
char *get_path_to_file(char *filename);
char *get_content_type(char *filename);
void *handle_http_request(void *arg);
void send_response_body(int newfd, int fd);
void send_response_head(int newfd, char *status_line, char *content_type);
char *make_content_type_header(char *content_type);



// Global variable, to be shared by all threads
char *path_to_web_root = NULL;



/* Gives user information on how
 * to use the program with corresponding
 * command line arguments.
 */
void usage(char *prog_name) {
    printf("Usage: %s [port number] [path to web root]\n", prog_name);
}


// Reads in a request from the file descriptor associated with
// a connection
char *get_request_line(FILE *fdstream) {

    if (fdstream == NULL) {
        perror("Error opening file descriptor");
        exit(EXIT_FAILURE);
    }

    ssize_t read;
    size_t len = 0;
    char *request_line = NULL;

    // Read the request line
    if ((read = getline(&request_line, &len, fdstream)) == -1) {
        perror("Error reading HTTP request line");
        exit(EXIT_FAILURE);
    }

    fflush(fdstream);
    return request_line;
}


/**
 * Parse the HTTP request line to get the name
 * of the file requested by the client.
 */
char *get_filename(char *request_line) {

    // Allocate and initialise memory for filename
    char *filename = malloc(sizeof(char)*strlen(request_line));
    if (filename == NULL) {
        perror("Error allocating memory for filename");
        exit(EXIT_FAILURE);
    }
    memset(filename, '\0', strlen(request_line));

    // Get the filename from the request line
    if (sscanf(request_line, "GET %s HTTP/1.0\n", filename) != 1) {
        fprintf(stderr, "Failed to scan request line for filename\n");
        exit(EXIT_FAILURE);
    }

    // If requested file contains "/" on RHS, set filename to "index.html"
    if (strcmp(filename+strlen(filename)-1, "/") == 0) {
        strcat(filename, "index.html");
    }

    return filename;
}


// Gets the path to the file requested. Can either be an absolute
// path or relative path, depending on the web root specified in
// command line.
char *get_path_to_file(char *filename) {

    // Calculate total length of path to the requested file
    size_t path_len = strlen(path_to_web_root) + strlen(filename) + 1;

    // Allocate space for the path name
    char *path;
    if ((path = malloc(sizeof(char)*path_len)) == NULL) {
        perror("Error allocating memory for path name");
        exit(EXIT_FAILURE);
    }
    
    // Create the path name from the web root and requested filename
    strcpy(path, path_to_web_root);
    strcat(path, filename);

    return path;
}




/* Parses the filename the client wants, and
 * returns the MIME type (content type) associated
 * with the file's extension.
 */
char *get_content_type(char *filename) {

    // The max length content type i.e. application/javascript
    const size_t max_content_type_len = 22;

    char *content_type = malloc(sizeof(char)*(max_content_type_len+1)); // +1 for nullbyte
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


/**
 * HTTP request handler function for a single worker thread.
 */
void *handle_http_request(void *arg) {

    int newfd = *((int*)arg);

    // Open the stream associated with the connection file descriptor
    FILE *fdstream = fdopen(newfd, "r");

    // Process the request, and extract relevant information
    char *request_line = get_request_line(fdstream);
    char *filename     = get_filename(request_line);
    char *content_type = get_content_type(filename);
    char *path_to_file = get_path_to_file(filename);


    // Send the appropriate HTTP response status line and headers,
    // based on whether the file exists on the server or not.
    int fd;
    if ((fd = open(path_to_file, O_RDONLY)) == -1) {

        printf("Unable to open file at %s\n", path_to_file);

        // File does not exist on server
        send_response_head(newfd, "HTTP/1.0 404 NOT FOUND\n", content_type);

    } else {

        printf("Successfully opened file %s\n", path_to_file);

        // File exists on server
        send_response_head(newfd, "HTTP/1.0 200 OK\n", content_type);
    }

    // Then send the appropriate HTTP response body, which
    // is either empty if the file does not exist on the server,
    // or the file contents if it does.
    send_response_body(newfd, fd);


    printf("\nSuccesfully sent requested file\n");

    // Free all allocated memory for this response
    free(request_line);
    free(filename);
    free(content_type);
    free(path_to_file);

    // Close the file stream associated with the
    // file descriptor describing the connection.
    fclose(fdstream);

    // Close the connection to the client
    close(newfd);

    // Terminate this thread
    pthread_exit(NULL);
}


/**
 * Constructs the status line and headers to be sent in the HTTP
 * response, and then actually sends them to the client.
 */
void send_response_head(int newfd, char *status_line, char *content_type) {    

    // First construct the 'Content-Type' header, using the given content type
    char *content_type_header = make_content_type_header(content_type);

    // Calculate the size of the response head, + 1 for newline.
    size_t size = strlen(status_line) + strlen(content_type_header) + 1;

    // Allocate memory for response, +1 for nullbyte
    char *response_head = malloc(sizeof(char)*(size+1));
    if (response_head == NULL) {
        perror("Error allocating memory to HTTP response");
        exit(EXIT_FAILURE);
    }

    // Initialise memory so we can write over it safely
    response_head[0] = '\0';


    // Construct the HTTP response message status, header, and empty lines
    strcat(response_head, status_line);
    strcat(response_head, content_type_header);
    strcat(response_head, "\n");
    

    // Now send the actual HTTP response status line and headers
    ssize_t total_bytes_sent = 0, bytes_sent;
    while ((bytes_sent = send(newfd, response_head+total_bytes_sent, 
                              size-total_bytes_sent, 0)) > 0) {
        total_bytes_sent += bytes_sent;
    }

    // Check to see if we've successfully sent the response head
    if (total_bytes_sent != size) {
        fprintf(stderr, "Error sending response head\n");
        exit(EXIT_FAILURE);
    }

    // Free the memory allocated to content type header and response head
    free(content_type_header);
    free(response_head);
}



/**
 * Takes as input the file descriptors for an open connection
 * with a client, and one for a file, and sends the contents
 * of the file to the client.
 */
void send_response_body(int newfd, int fd) {
    // Don't send anything if file does not exist,
    // empty message body.
    if (fd < 0) {
        return;
    }

    // The send buffer
    unsigned char buffer[BUFFER_SIZE];

    // Get the size of the file in bytes
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);


    printf("Size of file: %lu bytes\n", size);

    // The total bytes read from the file, used as 
    // an offset if file is not read in one go.
    ssize_t total_bytes_read = 0;
    ssize_t total_bytes_sent = 0;

    ssize_t bytes_sent;
    ssize_t bytes_read;

    // Read contents of the file, and send as we go.
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {

        total_bytes_read += bytes_read;

        // Send the contents to client
        if ((bytes_sent = send(newfd, buffer, bytes_read, 0)) < 0) {
            perror("Error sending buffer contents");
            exit(EXIT_FAILURE);
        }
        //printf("%s", buffer);
        total_bytes_sent += bytes_sent;
    }


    printf("Total bytes sent: %zu bytes\n", total_bytes_sent);

    // Check that we have correctly sent the file.
    if (total_bytes_read != size) {
        fprintf(stderr, "Error reading file\n");
        exit(EXIT_FAILURE);
    } else if (total_bytes_sent != size) {
        fprintf(stderr, "Error sending file\n");
        exit(EXIT_FAILURE);
    }
}



char *make_content_type_header(char *content_type) {
    // Sanity checking
    assert(content_type);

    // The base size of the 'Content-Type' header,
    // including two newline and one whitespace characters
    const size_t base_size = 15;

    // Calculate size of content type header (without nullbyte)
    size_t content_type_header_size = base_size + strlen(content_type);

    // Allocate and initialise memory for content type header
    char *content_type_header;
    content_type_header = malloc(sizeof(char)*(content_type_header_size+1));
    if (content_type_header == NULL) {
        perror("Error allocating memory for content type header");
        exit(EXIT_FAILURE);
    }
    content_type_header[0] = '\0';


    // Construct the header
    strcat(content_type_header, "Content-Type: ");
    strcat(content_type_header, content_type);
    strcat(content_type_header, "\n");

    return content_type_header;
}


/** MAIN FUNCTION **/
int main(int argc, char *argv[]) {

    // Check if command line arguments were supplied.
    if (argc < 3) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    } 

    // Get the port number and path to web root.
    char *port_num = argv[1];
    path_to_web_root = argv[2];


    int sockfd, newfd;
    struct addrinfo hints, *res;      // point to results


    memset(&hints, '\0', sizeof(hints)); // make sure struct is empty
    hints.ai_family   = AF_INET;        // use IPv4 address
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
    hints.ai_flags    = AI_PASSIVE;      // fill in my IP for me


    // Initialise structs with server information
    int status;
    if ((status = getaddrinfo(NULL, port_num, &hints, &res)) != 0) {
        perror("Error getting address information");
        fprintf(stderr, "%s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }


    // Iterate through the linked list of results, and bind
    // server to the first available socket.
    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        // Create a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 0)) < 0) {
            perror("Error creating socket");
            // Just have to skip this one!
            continue;
        }

        // Set option to allow reuse of a port if the server terminates.
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("Error setting socket options");
            exit(EXIT_FAILURE);
        }

        // Try binding the socket
        if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("Error binding socket");
            // Again, if it fails, simply close the socket and skip it!
            close(sockfd);
            continue;
        }
        break;
    }

    // Sanity check to see if we've successfully bound a socket.
    if (p == NULL) {
        fprintf(stderr, "Server failed to bind a socket\n");
        exit(EXIT_FAILURE);
    }


    // Free the linked list of results for IP addresses of host.
    freeaddrinfo(res);


    // DEBUGGING
    printf("Successfully created socket\n");
    printf("Successfully set socket options\n");
    printf("Successfully bound socket\n");


    // Listen, which allocates space to queue incoming calls for the
    // case that several clients try to connect at the same time. (non-blocking call)
    if (listen(sockfd, BACKLOG) < 0) {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }


    printf("Successfully listened on socket\n");



    // Socket is now set up and bound. Wait for connection and process it.
    // Use the accept function to retrieve a connect request and convert it
    // into a connection.


    // Main loop of server
    for (;;) {

        // Block until a connection request arrives
        newfd = accept(sockfd, (struct sockaddr*)NULL, NULL);

        // Check if connection was successfull
        if (newfd < 0) {
            perror("ERROR on accept");
            exit(EXIT_FAILURE);
        }

        printf("Successfully accepted a client connection request\n");


        pthread_t client_handler;
        pthread_create(&client_handler, NULL, handle_http_request, &newfd);

        // UNCOMMENT THIS SHIT FOR ONLY ONE REQUEST
        //pthread_join(client_handler, NULL);
        //break;
        
    }

    

    // If we don't care about a client's identity, can set client address and
    // length parameters (last 2 parameters to accept) as NULL. Otherwise,
    // before calling accept, we need to set addr parameter to a buffer large
    // enough to hold the address, and set the integer pointed to by len to
    // the size of the buffer in bytes.

    
    /* close socket */
    close(sockfd);

    return 0;
}