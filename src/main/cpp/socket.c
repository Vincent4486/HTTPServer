#include <stdlib.h> // exit, EXIT_FAILURE
#include <stdio.h>  // printf, perror
#include <string.h> // strlen, strcmp, strtok, strdup

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>     // read, write, close
#include <fcntl.h>      // open, O_RDONLY
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <sys/socket.h> // socket, bind, listen, accept
#include <arpa/inet.h>  // inet_addr
#endif

#include "include/socket.h"

int start_server(const char *host, int port)
{
#ifdef _WIN32
    WSADATA wsaData;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_result != 0)
    {
        fprintf(stderr, "WSAStartup failed: %d\n", wsa_result);
        exit(1);
    }
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        printf("#005\n");
        exit(4);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // Enhanced IP binding logic
    if (strcmp(host, "localhost") == 0)
    {
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else if (strcmp(host, "any") == 0 || strlen(host) == 0)
    {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        address.sin_addr.s_addr = inet_addr(host);
        if (address.sin_addr.s_addr == INADDR_NONE)
        {
            fprintf(stderr, "Invalid IP address: %s\n", host);
            printf("#004\n");
#ifdef _WIN32
            closesocket(server_fd);
            WSACleanup();
#else
            close(server_fd);
#endif
            exit(4);
        }
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        printf("#006\n");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        exit(4);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        printf("#007\n");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        exit(4);
    }

    printf("Server started on %s:%d\n", host, port);
    return server_fd;
}