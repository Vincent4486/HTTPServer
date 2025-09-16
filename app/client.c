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

    // Parse HTTP method and path
    char method[8], path[1024];
    sscanf(buffer, "%s %s", method, path);

    // Default to index.html if root is requested
    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }

    // Build full file path
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", content_directory, path);

    // Open file
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
        return;
    }

    // Get file size
    struct stat st;
    fstat(fd, &st);
    int file_size = st.st_size;

    // Read file content
    char *file_buffer = malloc(file_size);
    read(fd, file_buffer, file_size);
    close(fd);

    // Send HTTP response
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             file_size);

    write(client_fd, header, strlen(header));
    write(client_fd, file_buffer, file_size);

    free(file_buffer);
}

void run_server_loop(int server_fd, const char *content_directory)
{
    // Implementation of the server loop
    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        handle_client(client_fd, content_directory); // 🔥 This is your client handler
        close(client_fd);
    }
}
