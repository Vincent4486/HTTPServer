#ifndef PHP_H
#define PHP_H

#include <stddef.h>

void handle_php_request(const char *script_path,
                        int client_fd,
                        const char *method,
                        const char *query_string,
                        const char *content_type,
                        const char *remote_addr,
                        const char *server_name,
                        const char *server_software,
                        const char *body,
                        size_t body_len);

#endif