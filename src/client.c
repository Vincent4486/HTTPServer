#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strlen, strcpy, memset
#include <stdbool.h>    // bool, true, false
#include <stdint.h>     // intmax_t
#include <inttypes.h>   // PRIdMAX
#include <ctype.h>      // isxdigit

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include "include/compat.h"


#include <limits.h> // PATH_MAX

#include "include/client.h"
#include "include/logger.h"
#include "include/http.h"

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

int url_decode(char *s)
{
    char *dst = s;
    while (*s)
    {
        if (*s == '%')
        {
            if (!isxdigit((unsigned char)s[1]) || !isxdigit((unsigned char)s[2]))
                return -1;
            char hex[3] = {s[1], s[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            s += 3;
        }
        else if (*s == '+')
        {
            *dst++ = ' ';
            s++;
        }
        else
        {
            *dst++ = *s++;
        }
    }
    *dst = '\0';
    return 0;
}

void send_404(int client_fd)
{
    const char *not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "404 Not Found";
    write(client_fd, not_found, strlen(not_found));
}

void send_403(int client_fd)
{
    const char *forbidden =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Forbidden";
    write(client_fd, forbidden, strlen(forbidden));
}

void send_301_location(int client_fd, const char *location)
{
    char hdr[1024];
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 301 Moved Permanently\r\n"
                     "Location: %s\r\n"
                     "Content-Length: 0\r\n"
                     "\r\n",
                     location);
    if (n > 0)
        write(client_fd, hdr, n);
}

/* small helper to write a 200 header */
void send_200_header(int client_fd, const char *mime, off_t len)
{
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %jd\r\n"
                              "\r\n",
                              mime, (intmax_t)len);
    if (header_len > 0)
        write(client_fd, header, header_len);
}

const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";
    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";
    return "application/octet-stream";
}

int stream_file_fd(int client_fd, int fd, off_t filesize)
{
    ssize_t r;
    const size_t BUF_SZ = 16 * 1024;
    char *buf = malloc(BUF_SZ);
    if (!buf)
        return -1;
    off_t remaining = filesize;
    while (remaining > 0)
    {
        size_t toread = remaining > (off_t)BUF_SZ ? BUF_SZ : (size_t)remaining;
        r = read(fd, buf, toread);
        if (r <= 0)
        {
            free(buf);
            return -1;
        }
        ssize_t w = 0;
        char *p = buf;
        while (r > 0)
        {
            ssize_t wn = write(client_fd, p, r);
            if (wn <= 0)
            {
                free(buf);
                return -1;
            }
            r -= wn;
            p += wn;
        }
        remaining -= (off_t)(p - buf);
    }
    free(buf);
    return 0;
}

#define TYPE_HTML 0
#define TYPE_PHP 1
#define TYPE_PERL 2
#define TYPE_UNKNOWN 3

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int determine_file_type(const char *resolved_path)
{
    char path[PATH_MAX];
    size_t len;

    if (!resolved_path)
        return TYPE_UNKNOWN;

    // copy and normalize path
    if (strlen(resolved_path) >= sizeof(path))
        return TYPE_UNKNOWN;
    strcpy(path, resolved_path);
    len = strlen(path);

    // remove trailing slashes except keep root "/"
    while (len > 1 && path[len - 1] == '/')
    {
        path[len - 1] = '\0';
        --len;
    }

    // If path is a directory, try index files inside it
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        char test[PATH_MAX];

        snprintf(test, sizeof(test), "%s%sindex.html", path, PATH_SEPARATOR);
        if (file_exists(test))
            return TYPE_HTML;

        snprintf(test, sizeof(test), "%s%sindex.php", path, PATH_SEPARATOR);
        if (file_exists(test))
            return TYPE_PHP;

        snprintf(test, sizeof(test), "%s%sindex.pl", path, PATH_SEPARATOR);
        if (file_exists(test))
            return TYPE_PERL;

        return TYPE_UNKNOWN;
    }

    // If the provided path is a regular file, check its extension
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    {
        const char *ext = strrchr(path, '.');
        if (ext)
        {
            if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
                return TYPE_HTML;
            if (strcasecmp(ext, ".php") == 0)
                return TYPE_PHP;
            if (strcasecmp(ext, ".pl") == 0)
                return TYPE_PERL;
        }
        return TYPE_UNKNOWN;
    }

    // If the path doesn't exist as-is, try appending common extensions
    {
        char test[PATH_MAX];

        snprintf(test, sizeof(test), "%s.html", path);
        if (file_exists(test))
            return TYPE_HTML;

        snprintf(test, sizeof(test), "%s.php", path);
        if (file_exists(test))
            return TYPE_PHP;

        snprintf(test, sizeof(test), "%s.pl", path);
        if (file_exists(test))
            return TYPE_PERL;

        // also try directory form: path/ -> path/index.*
        snprintf(test, sizeof(test), "%s/index.html", path);
        if (file_exists(test))
            return TYPE_HTML;

        snprintf(test, sizeof(test), "%s/index.php", path);
        if (file_exists(test))
            return TYPE_PHP;

        snprintf(test, sizeof(test), "%s/index.pl", path);
        if (file_exists(test))
            return TYPE_PERL;
    }

    return TYPE_UNKNOWN;
}

/* join directory and request path safely into out (must be PATH_MAX). returns 0 on success */
int join_path(const char *dir, const char *req, char *out, size_t outlen)
{
    if (!dir || !req || !out)
        return -1;
    size_t dlen = strlen(dir);
    if (dlen >= outlen)
        return -1;
    if (dir[dlen - 1] == '/')
    {
        if (snprintf(out, outlen, "%s%s", dir, req[0] == '/' ? req + 1 : req) >= (int)outlen)
            return -1;
    }
    else
    {
        if (snprintf(out, outlen, "%s%s", dir, req) >= (int)outlen)
            return -1;
    }
    return 0;
}

void run_server_loop(int server_fd, const char *content_directory, const bool show_ext)
{
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != 0) {
        log_error_code(12, "Failed to initialize Winsock"); /* #012 */
        return;
    }
#endif

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "accept() failed: %s", strerror(errno));
            log_error_code(15, "%s", err_msg); /* #015 */
            continue;
        }

        // Extract client IP and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Accepted connection from %s:%d", client_ip, client_port);
        log_info(log_msg);

        int request_type = determine_file_type(content_directory);
        if (request_type == 0)
        {
            handle_http_request(client_fd, content_directory, show_ext);
        }
        else if (request_type == 1)
        {
            // handle_php_request(client_fd, content_directory);
        }
        else if (request_type == 2)
        {
            // handle perl request
        }
        else
        {
            // unknown file type
            send_404(client_fd);
            close(client_fd);
            continue;
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
}