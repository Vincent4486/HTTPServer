#include <stdio.h>  // printf, perror
#include <stdlib.h> // exit, malloc
#include <string.h> // strlen, strcmp, strtok
#include <unistd.h> // read, write, close
#include <fcntl.h>  // open, O_RDONLY
#include <netinet/in.h>

#include "include/setting.h"

int main()
{
    const char *dir = get_server_directory();
    int port = get_server_port();
    printf("Server Directory: %s\n", dir);
    printf("Server Port: %d\n", port);
    return 0;
}