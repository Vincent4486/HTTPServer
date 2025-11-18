// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include <time.h>
#include <limits.h>
#include "compat.h"

#define CACHE_MAX_ENTRIES 32
#define CACHE_MAX_FILE_SIZE (64 * 1024) // 64KB max cached file size

typedef struct
{
    char path[PATH_MAX];
    char *data;
    size_t size;
    const char *mime_type;
    time_t mtime;
    time_t cached_at;
} cache_entry_t;

void run_server_loop(int server_fd, const char *content_directory, const bool show_ext);

/* Handle an accepted client connection */
void handle_accepted_client(int client_fd, struct sockaddr_in client_addr,
                            const char *content_directory, const bool show_ext);

/* Forward declaration for thread pool */
typedef struct threadpool threadpool_t;

/* Run server loop with thread pool */
void run_server_loop_with_threadpool(int server_fd, const char *content_directory, const bool show_ext, threadpool_t *pool);
int url_decode(char *s);
void send_404(int client_fd);
void send_403(int client_fd);
void send_304(int client_fd);
void send_206_header(int client_fd, const char *mime, off_t range_start, off_t range_end, off_t total_size);
void send_301_location(int client_fd, const char *location);
void send_200_header(int client_fd, const char *mime, off_t len);
void send_200_header_keepalive(int client_fd, const char *mime, off_t len);
const char *get_mime_type(const char *path);
int stream_file_fd(int client_fd, int fd, off_t filesize);
int join_path(const char *dir, const char *req, char *out, size_t outlen);
int write_buffer_fully(int client_fd, const char *buf, ssize_t size);
cache_entry_t *cache_get(const char *path);
void cache_put(const char *path, const char *data, size_t size, const char *mime_type, time_t mtime);
void cache_init(void);

/* HTTP header parsing helpers */
int get_if_modified_since(const char *request_buf, time_t *out_time);
int parse_range_header(const char *request_buf, off_t file_size, off_t *out_start, off_t *out_end);

#endif // CLIENT_H