// main.c
#include <stdio.h>  // printf, perror
#include <stdlib.h> // exit, malloc
#include <string.h> // strlen, strcmp, strtok
#include <unistd.h> // read, write, close
#include <fcntl.h>  // open, O_RDONLY
#include <netinet/in.h>

#include "settings.h"
#include "socket.h"
#include "client.h"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *server_content_directory = get_server_directory();
    int server_port = get_server_port();
    const char *server_host = get_server_host();
    printf("Server Directory: %s\n", server_content_directory);
    printf("Server Port: %d\n", server_port);
    printf("Server Host: %s\n", server_host);

    int server_fd = start_server(server_host, server_port);
    run_server_loop(server_fd, server_content_directory);

    return 0;
}