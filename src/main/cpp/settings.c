#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>

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
#include "include/logger.h"

static cJSON *cached_config = NULL;

static bool directory_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

char *get_config_path()
{
    static char path[1024];
    static char config_path[1024];

#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, path, sizeof(path)))
    {
        log_error("Failed to get executable path");
        exit(EXIT_FAILURE);
    }
    char *last_backslash = strrchr(path, '\\');
    if (last_backslash)
        *last_backslash = '\0';
#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0)
    {
        log_error("Executable path too long");
        exit(EXIT_FAILURE);
    }
    char *last_slash = strrchr(path, '/');
    if (last_slash)
        *last_slash = '\0';
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1)
    {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "readlink failed: %s", strerror(errno));
        log_error(err_msg);
        exit(EXIT_FAILURE);
    }
    path[len] = '\0';
    char *last_slash = strrchr(path, '/');
    if (last_slash)
        *last_slash = '\0';
#endif

    snprintf(config_path, sizeof(config_path), "%s/config.json", path);
    return config_path;
}

static void load_config()
{
    if (cached_config)
        return;

    char *filepath = get_config_path();
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to open config.json: %s", strerror(errno));
        log_error(err_msg);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = malloc(length + 1);
    if (!data)
    {
        log_error("Memory allocation failed while reading config");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cached_config = cJSON_Parse(data);
    free(data);

    if (!cached_config)
    {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Error parsing JSON: %s", cJSON_GetErrorPtr());
        log_error(err_msg);
        exit(EXIT_FAILURE);
    }
}

const int get_server_port()
{
    load_config();
    cJSON *port = cJSON_GetObjectItemCaseSensitive(cached_config, "server-port");
    if (!cJSON_IsNumber(port))
    {
        log_error("Port not found or not a number in config.json");
        exit(EXIT_FAILURE);
    }
    return port->valueint;
}

const char *get_server_directory()
{
    load_config();
    cJSON *dir = cJSON_GetObjectItemCaseSensitive(cached_config, "server-content-directory");
    if (!cJSON_IsString(dir) || dir->valuestring == NULL)
    {
        log_error("Directory not found or not a string in config.json");
        exit(EXIT_FAILURE);
    }

    const char *path = dir->valuestring;
    if (strcmp(path, "default") == 0)
    {
        char *executable_dir = get_config_path();
        char *last_slash = strrchr(executable_dir, '/');
        if (last_slash)
            *last_slash = '\0';

        static char default_dir[1024];
        snprintf(default_dir, sizeof(default_dir), "%s/server-content", executable_dir);

        if (!directory_exists(default_dir))
        {
            if (mkdir(default_dir, 0700) != 0)
            {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Failed to create default directory: %s", default_dir);
                log_error(err_msg);
                exit(EXIT_FAILURE);
            }
            else
            {
                char success_msg[256];
                snprintf(success_msg, sizeof(success_msg), "Default directory created: %s", default_dir);
                log_info(success_msg);
            }
        }

        return strdup(default_dir);
    }
    else if (!directory_exists(path))
    {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Directory does not exist: %s", path);
        log_error(err_msg);
        exit(EXIT_FAILURE);
    }

    return strdup(path);
}

const char *get_server_host()
{
    load_config();
    cJSON *host = cJSON_GetObjectItemCaseSensitive(cached_config, "server-host");
    if (!cJSON_IsString(host) || host->valuestring == NULL)
    {
        log_error("Host not found or not a string in config.json");
        exit(EXIT_FAILURE);
    }
    return strdup(host->valuestring);
}

const bool get_show_file_extension()
{
    load_config();
    cJSON *show_ext = cJSON_GetObjectItemCaseSensitive(cached_config, "show-file-extension");
    return cJSON_IsBool(show_ext) ? (show_ext->valueint != 0) : false;
}