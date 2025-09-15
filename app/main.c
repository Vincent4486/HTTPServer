#include <stdio.h>  // printf, perror
#include <stdlib.h> // exit, malloc
#include <string.h> // strlen, strcmp, strtok
#include <unistd.h> // read, write, close
#include <fcntl.h>  // open, O_RDONLY
#include <netinet/in.h>

#include "include/properties.h"

int main()
{
    char dir = serverDir();
    printf("%s\n", dir);
    return 0;
}