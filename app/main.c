#include <stdio.h>  // printf, perror
#include <stdlib.h> // exit, malloc
#include <string.h> // strlen, strcmp, strtok
#include <unistd.h> // read, write, close
#include <fcntl.h>  // open, O_RDONLY
#include <netinet/in.h>

#include "include/properties.h"

int main()
{
    const char *dir = get_server_directory();
    return 0;
}