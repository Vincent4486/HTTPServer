// client.h
#ifndef CLIENT_H
#define CLIENT_H

void run_server_loop(int server_fd, const char *content_directory, const bool show_ext);
int url_decode(char *s);
void send_404(int client_fd);
void send_403(int client_fd);
void send_301_location(int client_fd, const char *location);
void send_200_header(int client_fd, const char *mime, off_t len);
const char *get_mime_type(const char *path);
int stream_file_fd(int client_fd, int fd, off_t filesize);
int join_path(const char *dir, const char *req, char *out, size_t outlen);

#endif // CLIENT_H