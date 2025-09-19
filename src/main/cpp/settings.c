#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #ifdef __APPLE__
        #include <mach-o/dyld.h>
    #endif
#endif

#ifdef _WIN32
    #include "lib/cJSON.h"
#else
    #include <cjson/cJSON.h>
#endif

#include "include/settings.h"

static cJSON *cached_config = NULL;

static bool directory_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

char *get_config_path() {
    static char path[1024];
    static char config_path[1024];

#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, path, sizeof(path))) {
        fprintf(stderr, "Failed to get executable path\n");
        exit(1);
    }
    char *last_backslash = strrchr(path, '\\');
    if (last_backslash) *last_backslash = '\0';
#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        fprintf(stderr, "Executable path too long\n");
        exit(1);
    }
    char *last_slash = strrchr(path, '/');
    if (last_slash) *last_slash = '\0';
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) {
        perror("readlink");
        exit(1);
    }
    path[len] = '\0';
    char *last_slash = strrchr(path, '/');
    if (last_slash) *last_slash = '\0';
#endif

    snprintf(config_path, sizeof(config_path), "%s/config.json", path);
    return config_path;
}

static void load_config() {
    if (cached_config) return;

    char *filepath = get_config_path();
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open config.json");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = malloc(length + 1);
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cached_config = cJSON_Parse(data);
    free(data);

    if (!cached_config) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        exit(1);
    }
}

const int get_server_port() {
    load_config();
    cJSON *port = cJSON_GetObjectItemCaseSensitive(cached_config, "server-port");
    if (!cJSON_IsNumber(port)) {
        fprintf(stderr, "Port not found or not a number\n");
        exit(1);
    }
    return port->valueint;
}

const char *get_server_directory() {
    load_config();
    cJSON *dir = cJSON_GetObjectItemCaseSensitive(cached_config, "server-content-directory");
    if (!cJSON_IsString(dir) || dir->valuestring == NULL) {
        fprintf(stderr, "Directory not found or not a string\n");
        exit(1);
    }

    const char *path = dir->valuestring;
    if (!directory_exists(path)) {
        fprintf(stderr, "Directory does not exist: %s\n", path);
        exit(1);
    }

    return strdup(path);
}


const char *get_server_host() {
    load_config();
    cJSON *host = cJSON_GetObjectItemCaseSensitive(cached_config, "server-host");
    if (!cJSON_IsString(host) || host->valuestring == NULL) {
        fprintf(stderr, "Host not found or not a string\n");
        exit(1);
    }
    return strdup(host->valuestring);
}
