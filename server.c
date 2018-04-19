#include <sys/types.h>
#include <sys/fcntl.h>    // need for open O_ constants
#include <sys/socket.h>   // needed for socket function
#include <netinet/in.h>   // needed for internet address i.e. sockaddr_in and in_addr
#include <netdb.h>        // needed for setting up server i.e. AI_PASSIVE and shit
#include <assert.h>
#include <arpa/inet.h>  // needed for byte order functions
#include <unistd.h>     // needed for read/close functions
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>    // for POSIX threads
#include "server.h"



/** 
 * Gives the user information on how to use the program 
 * and the required input command line arguments.
 */
void usage(char *prog_name) {
    printf("Usage: %s [port number] [path to web root]\n", prog_name);
}


/**
 * Extracts and returns the HTTP request line from a
 * HTTP request sent by a client over a connection.
 */
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

    // Allocate and initialise memory for filename.
    // We know that the filename will fit into the
    // size occupied by the entire request line.
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



/**
 * Parses the name of the file the client requested
 * and returns the MIME type (content type) associated
 * with the file.
 */
char *get_content_type(char *filename) {

    // The max length content type i.e. application/javascript
    const size_t max_content_type_len = 22;

    // Allocate and initialise memory for the content type.
    char *content_type = malloc(sizeof(char)*(max_content_type_len+1)); // +1 for nullbyte
    if (content_type == NULL) {
        perror("Failed to allocate memory to content type");
        exit(EXIT_FAILURE);
    }
    memset(content_type, '\0', max_content_type_len+1);

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
 * Given a content type,
 */
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


/**
 * Sends a message properly through the TCP connection socket.
 * Returns the total number of bytes sent.
 */
ssize_t send_message(int newfd, char *message, ssize_t size) {

    // Attempt to send full contents of the message.
    ssize_t bytes_sent, offset = 0;
    while ((bytes_sent = send(newfd, message+offset, size-offset, 0)) > 0) {
        offset += bytes_sent;
    }

    // Check that we have sent everything.
    if (size != offset || bytes_sent < 0) {
        perror("Failed to send message");
        exit(EXIT_FAILURE);
    }

    // If we have reached this point, offset is
    // the total number of bytes sent.
    return offset;
}



/**
 * Gets the path to the file requested. Can either be an absolute
 * path or relative path, depending on the web root specified on
 * the command line.
 */
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
    response_head[0] = '\0';

    // Construct the HTTP response message status, header, and empty lines
    strcat(response_head, status_line);
    strcat(response_head, content_type_header);
    strcat(response_head, "\n");
    
    // Now send the actual HTTP response status line and headers.
    ssize_t total_bytes_sent = send_message(newfd, response_head, size);

    // Check to see if we've successfully sent the response head.
    if (total_bytes_sent != size) {
        fprintf(stderr, "Error sending response head\n");
        exit(EXIT_FAILURE);
    }

    // Free the memory allocated to content type header and response head.
    free(content_type_header);
    free(response_head);
}



/**
 * Takes as input the file descriptors for an open connection
 * with a client, and one for a file, and sends the contents
 * of the file to the client.
 */
void send_response_body(int newfd, int fd) {

    // Don't send anything if file does not exist.
    if (fd < 0) {
        return;
    }

    // Get the size of the file in bytes.
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    printf("Size of file: %lu bytes\n", size);

    // Counts used for error checking.
    ssize_t total_bytes_read = 0;
    ssize_t total_bytes_sent = 0;
    ssize_t bytes_read;
    ssize_t bytes_sent;

    // The send buffer.
    char buffer[BUFFER_SIZE];

    // Read contents of the file, and send as we go.
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {

        // Attempt to send message, and get the number of bytes sent.
        bytes_sent = send_message(newfd, buffer, bytes_read);

        // Update the total counts.
        total_bytes_sent += bytes_sent;
        total_bytes_read += bytes_read;
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



/**
 * HTTP request handler function for a single worker thread.
 * Responsible for fully constructing and sending a valid
 * HTTP response to the client, after processing the request.
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

        // Set options to allow reuse of a port if the server terminates.
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
            perror("Error on accept");
            continue;
        }

        printf("Successfully accepted a client connection request\n");

        pthread_t client_handler;
        pthread_create(&client_handler, NULL, handle_http_request, &newfd);
    }

    // Close the socket
    close(sockfd);

    // Done! 
    return 0;
}