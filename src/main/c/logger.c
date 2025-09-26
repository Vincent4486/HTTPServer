#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <mach-o/dyld.h>
#endif

#include "include/logger.h"

static char log_file_path[PATH_MAX] = "";

static void get_executable_dir(char *buffer, size_t size)
{
#ifdef _WIN32
    char exe_path[PATH_MAX];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len == 0 || len == sizeof(exe_path))
    {
        fprintf(stderr, "Failed to get executable path\n");
        exit(EXIT_FAILURE);
    }

    // Strip filename to get directory
    char *last_slash = strrchr(exe_path, '\\');
    if (last_slash)
        *last_slash = '\0';
    strncpy(buffer, exe_path, size);
#else
    char exe_path[PATH_MAX];
    uint32_t len = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &len) != 0)
    {
        fprintf(stderr, "Executable path too long\n");
        exit(EXIT_FAILURE);
    }

    char resolved[PATH_MAX];
    if (!realpath(exe_path, resolved))
    {
        perror("realpath failed");
        exit(EXIT_FAILURE);
    }

    char *last_slash = strrchr(resolved, '/');
    if (last_slash)
        *last_slash = '\0';
    strncpy(buffer, resolved, size);
#endif
}

static void initialize_log_file()
{
    if (log_file_path[0] != '\0')
        return;

    char exe_dir[PATH_MAX];
    get_executable_dir(exe_dir, sizeof(exe_dir));

    char log_dir[PATH_MAX];
#ifdef _WIN32
    snprintf(log_dir, sizeof(log_dir), "%s\\log", exe_dir);
#else
    snprintf(log_dir, sizeof(log_dir), "%s/log", exe_dir);
#endif

    // Create log directory if needed
    struct stat st = {0};
    if (stat(log_dir, &st) == -1)
    {
#ifdef _WIN32
        if (mkdir(log_dir) == -1)
        {
#else
        if (mkdir(log_dir, 0700) == -1)
        {
#endif
            perror("Failed to create log directory");
            exit(EXIT_FAILURE);
        }
    }

    // Format log file path
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
#ifdef _WIN32
    snprintf(log_file_path, sizeof(log_file_path), "%s\\%04d-%02d-%02d.log",
             log_dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
#else
    snprintf(log_file_path, sizeof(log_file_path), "%s/%04d-%02d-%02d.log",
             log_dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
#endif
}

static void log_to_file(const char *level, const char *message)
{
    initialize_log_file();

    FILE *file = fopen(log_file_path, "a");
    if (!file)
    {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", t);

    fprintf(file, "%s [%s] %s\n", time_buffer, level, message);
    fclose(file);
}

void log_info(const char *message)
{
    printf("[INFO] %s\n", message);
    log_to_file("INFO", message);
}

void log_error(const char *message)
{
    fprintf(stderr, "[ERROR] %s\n", message);
    log_to_file("ERROR", message);
}