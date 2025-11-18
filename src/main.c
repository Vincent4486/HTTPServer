#include <stdio.h>   // printf, perror
#include <stdlib.h>  // exit, malloc
#include <string.h>  // strlen, strcmp, strtok
#include <stdbool.h> // bool

#include "include/compat.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#include "include/settings.h"
#include "include/socket.h"
#include "include/client.h"
#include "include/logger.h"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

#ifdef _WIN32
    WSADATA wsaData;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_result != 0)
    {
        fprintf(stderr, "WSAStartup failed: %d\n", wsa_result);
        return 1;
    }
#endif

    /* Initialize file cache */
    cache_init();

    const char *server_content_directory = get_server_directory();
    int server_port = get_server_port();
    const char *server_host = get_server_host();
    const bool show_file_ext = get_show_file_extension();

    log_info("Server Directory: ");
    log_info(server_content_directory);
    char port_message[50];
    snprintf(port_message, sizeof(port_message), "Server Port: %d", server_port);
    log_info(port_message);
    log_info("Server Host: ");
    log_info(server_host);
    if (show_file_ext)
        log_info("File Extension Mode: SHOW-EXTENSION");
    else
        log_info("File Extension Mode: HIDE-EXTENSION");
    log_info("Reminder: When changed file extension mode to hide file extensions, files wit extensions will still work, please clear browser history to have the new version as default.");

    int server_fd = start_server(server_host, server_port);
    run_server_loop(server_fd, server_content_directory, show_file_ext);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
