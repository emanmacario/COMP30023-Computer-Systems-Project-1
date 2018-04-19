#ifndef SERVER_H
#define SERVER_H

// Constants
#define BUFFER_SIZE 8192  // buffer size
#define BACKLOG 10        // total pending connections queue will hold

// Global variable, to be shared by all threads
char *path_to_web_root;

// Function prototypes
char *get_content_type(char *filename);
void usage(char *prog_name);
char *get_request_line(FILE *fdstream);
char *get_filename(char *request_line);
char *get_path_to_file(char *filename);
char *get_content_type(char *filename);
void *handle_http_request(void *arg);
ssize_t send_message(int newfd, char *message, ssize_t size);
void send_response_body(int newfd, int fd);
void send_response_head(int newfd, char *status_line, char *content_type);
char *make_content_type_header(char *content_type);


#endif