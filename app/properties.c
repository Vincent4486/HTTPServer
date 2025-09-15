#include <sys/stat.h> // struct stat, stat
#include <stdbool.h>  // getenv
#include <stdlib.h>   // exit, EXIT_FAILURE
#include <stdio.h>    // printf

#include "include/properties.h"

static bool directory_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

const char *get_server_directory()
{
    const char *serverDir = "hello";
    if (directory_exists(serverDir))
    {
        return serverDir;
    }
    else
    {
        printf("Error: Server directory does not exist.\n#001\n");
        exit(EXIT_FAILURE);
    }
}