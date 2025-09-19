#include <stdio.h>   // printf, perror
#include <stdlib.h>  // exit, malloc
#include <string.h>  // strlen, strcmp, strtok
#include <stdbool.h> // bool

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>    // _read, _write, _close
#include <fcntl.h> // _O_RDONLY
#define read _read
#define write _write
#define close _close
#define open _open
#define O_RDONLY _O_RDONLY
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h> // read, write, close
#include <fcntl.h>  // open, O_RDONLY
#include <netinet/in.h>
#endif

#include "include/settings.h"
#include "include/socket.h"
#include "include/client.h"

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

    const char *server_content_directory = get_server_directory();
    int server_port = get_server_port();
    const char *server_host = get_server_host();
    const bool show_file_ext = get_show_file_extension();

    printf("Server Directory: %s\n", server_content_directory);
    printf("Server Port: %d\n", server_port);
    printf("Server Host: %s\n", server_host);
    if (show_file_ext)
        printf("File Extension Mode: SHOW-EXTENSION\n");
    else
        printf("File Extension Mode: HIDE-EXTENSION\n");
    printf("Reminder: When changed file extension mode to hide file extensions, "
           "files wit extensions will still work, please clear browser history to have "
           "the new version as default.\n");

    int server_fd = start_server(server_host, server_port);
    run_server_loop(server_fd, server_content_directory, show_file_ext);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
