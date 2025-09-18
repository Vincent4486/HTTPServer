// settings.c
#include <sys/stat.h> // struct stat, stat
#include <stdbool.h>  // getenv
#include <stdlib.h>   // exit, EXIT_FAILURE
#include <stdio.h>    // printf
#include <cjson/cJSON.h>
#include <libgen.h>      // dirname
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <string.h>      // strlen, strcmp, strtok, strdup

#include "settings.h"

static bool directory_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

char *get_config_path()
{
    static char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0)
    {
        fprintf(stderr, "Executable path too long\n");
        exit(002);
    }

    char *dir = dirname(path);
    static char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/config.json", dir);
    return config_path;
}

const int get_server_port()
{
    char *filepath = get_config_path();
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        perror("Failed to open config.json");
        printf("#002\n");
        exit(002);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = (char *)malloc(length + 1);
    if (!data)
    {
        fprintf(stderr, "Memory allocation failed\n");
        printf("#002\n");
        exit(002);
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        printf("#002\n");
        exit(002);
    }

    cJSON *port = cJSON_GetObjectItemCaseSensitive(json, "server-port");
    if (!cJSON_IsNumber(port))
    {
        fprintf(stderr, "Port not found or not a number\n");
        printf("#002\n");
        cJSON_Delete(json);
        exit(002);
    }

    int result = port->valueint;
    cJSON_Delete(json);
    return result;
}

const char *get_server_directory()
{
    char *filepath = get_config_path();
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        perror("Failed to open config.json");
        printf("#003\n");
        exit(003);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = (char *)malloc(length + 1);
    if (!data)
    {
        fprintf(stderr, "Memory allocation failed\n");
        printf("#003\n");
        exit(003);
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        printf("#003\n");
        exit(003);
    }

    cJSON *directory = cJSON_GetObjectItemCaseSensitive(json, "server-content-directory");
    if (!cJSON_IsString(directory) || directory->valuestring == NULL)
    {
        fprintf(stderr, "Directory not found or not a string\n");
        printf("#003\n");
        cJSON_Delete(json);
        exit(003);
    }

    const char *result = strdup(directory->valuestring); // Copy to preserve after deletion
    cJSON_Delete(json);
    return result;
}

const char *get_server_host()
{
    char *filepath = get_config_path();
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        perror("Failed to open config.json");
        printf("#004\n");
        exit(004);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = (char *)malloc(length + 1);
    if (!data)
    {
        fprintf(stderr, "Memory allocation failed\n");
        printf("#004\n");
        exit(004);
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        printf("#004\n");
        exit(004);
    }

    cJSON *host = cJSON_GetObjectItemCaseSensitive(json, "server-host");
    if (!cJSON_IsString(host) || host->valuestring == NULL)
    {
        fprintf(stderr, "Host not found or not a string\n");
        printf("#004\n");
        cJSON_Delete(json);
        exit(004);
    }

    const char *result = strdup(host->valuestring); // Copy to preserve after deletion
    cJSON_Delete(json);
    return result;
}