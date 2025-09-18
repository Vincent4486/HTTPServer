#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strlen, strcpy, memset
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // sockaddr_in, inet_addr
#include <sys/socket.h> // socket, connect
#include <fcntl.h>      // open, O_RDONLY
#include <sys/stat.h>   // stat
#include <errno.h>      // errno

#include "client.h"

const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";
    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";
    return "application/octet-stream";
}

void handle_client(int client_fd, const char *content_directory)
{
    char buffer[4096];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    char method[8], path[1024];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }

    if (strstr(path, ".."))
    {
        const char *forbidden =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "Forbidden";
        write(client_fd, forbidden, strlen(forbidden));
        close(client_fd);
        return;
    }

    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", content_directory, path);

    int fd = open(full_path, O_RDONLY);
    if (fd < 0)
    {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 Not Found";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    struct stat st;
    fstat(fd, &st);
    int file_size = st.st_size;

    char *file_buffer = malloc(file_size);
    if (!file_buffer)
    {
        close(fd);
        close(client_fd);
        return;
    }

    read(fd, file_buffer, file_size);
    close(fd);

    const char *mime_type = get_mime_type(path);
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             mime_type, file_size);

    write(client_fd, header, strlen(header));
    write(client_fd, file_buffer, file_size);

    free(file_buffer);
    close(client_fd);
}

void run_server_loop(int server_fd, const char *content_directory)
{
    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        handle_client(client_fd, content_directory);
    }
}