// socket.c
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <stdio.h>      // printf, perror
#include <string.h>     // strlen, strcmp, strtok, strdup
#include <unistd.h>     // read, write, close
#include <fcntl.h>      // open, O_RDONLY
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <sys/socket.h> // socket, bind, listen, accept
#include <arpa/inet.h>  // inet_addr

#include "socket.h"

int start_server(const char *host, int port)
{
    // Placeholder implementation
    // In a real implementation, you would set up sockets and start listening here
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1)
    {
        perror("Socket creation failed");
        printf("#005\n");
        exit(004);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(host);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        printf("#006\n");
        exit(004);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        printf("#007\n");
        exit(004);
    }
    printf("Server started on %s:%d\n", host, port);

    return server_fd;
}