#ifndef SERVER_H
#define SERVER_H

// Constant values.
#define BUFFER_SIZE 8192  // Buffer size
#define BACKLOG 10        // Total pending connections queue will hold

// The path to the web root is a global
// variable, to be shared by all threads.
char *path_to_web_root;

// Function prototypes.
void usage(char *prog_name);
int initialise_server(char *port_num);
char *get_request_line(FILE *fdstream);
char *get_filename(char *request_line);
char *get_content_type(char *filename);
char *get_path_to_file(char *filename);
char *make_content_type_header(char *content_type);
ssize_t send_message(int newfd, char *message, ssize_t size);
void send_response_head(int newfd, char *status_line, char *content_type);
void send_response_body(int newfd, int fd);
void *handle_http_request(void *arg);

#endif