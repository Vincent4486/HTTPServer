#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>

void handle_http_request(int client_fd, const char *client_ip, const char *content_directory, bool show_ext);

#endif