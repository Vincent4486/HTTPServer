// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include <time.h>

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
int url_decode(char *s);
void send_404(int client_fd);
void send_403(int client_fd);
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

#endif // CLIENT_H