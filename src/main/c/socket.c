#include <stdlib.h> // exit, EXIT_FAILURE
#include <stdio.h>  // snprintf
#include <string.h> // strlen, strcmp, strtok, strdup
#include <errno.h>  // errno

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
#include "include/logger.h"

int start_server(const char *host, int port)
{
#ifdef _WIN32
    WSADATA wsaData;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_result != 0)
    {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "WSAStartup failed: %d", wsa_result);
        log_error(err_msg);
        exit(EXIT_FAILURE);
    }
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
#ifdef _WIN32
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Socket creation failed: %d", WSAGetLastError());
#else
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Socket creation failed: %s", strerror(errno));
#endif
        log_error(err_msg);
        log_error("#005");
        exit(EXIT_FAILURE);
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
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "Invalid IP address: %s", host);
            log_error(err_msg);
            log_error("#004");
#ifdef _WIN32
            closesocket(server_fd);
            WSACleanup();
#else
            close(server_fd);
#endif
            exit(EXIT_FAILURE);
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
#ifdef _WIN32
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Bind failed: %d", WSAGetLastError());
#else
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Bind failed: %s", strerror(errno));
#endif
        log_error(err_msg);
        log_error("#006");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
#ifdef _WIN32
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Listen failed: %d", WSAGetLastError());
#else
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Listen failed: %s", strerror(errno));
#endif
        log_error(err_msg);
        log_error("#007");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        exit(EXIT_FAILURE);
    }

    char info_msg[128];
    snprintf(info_msg, sizeof(info_msg), "Server started on %s:%d", host, port);
    log_info(info_msg);

    return server_fd;
}