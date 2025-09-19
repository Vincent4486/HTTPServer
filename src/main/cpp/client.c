#include <stdio.h>    // printf, perror
#include <stdlib.h>   // exit, EXIT_FAILURE
#include <string.h>   // strlen, strcpy, memset
#include <stdbool.h>  // bool, true, fals
#include <stdint.h>   // intmax_t
#include <inttypes.h> // PRIdMAX
#include <ctype.h>    // isxdigit

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>       // _read, _write, _close
#include <fcntl.h>    // _O_RDONLY
#include <sys/stat.h> // stat
#include <errno.h>    // errno
#define read _read
#define write _write
#define close _close
#define open _open
#define O_RDONLY _O_RDONLY
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // sockaddr_in, inet_addr
#include <sys/socket.h> // socket, connect
#include <fcntl.h>      // open, O_RDONLY
#include <sys/stat.h>   // stat
#include <errno.h>      // errno
#endif

#include "include/client.h"

static int url_decode(char *s)
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

static void send_404(int client_fd)
{
    const char *not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "404 Not Found";
    write(client_fd, not_found, strlen(not_found));
}

static void send_403(int client_fd)
{
    const char *forbidden =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Forbidden";
    write(client_fd, forbidden, strlen(forbidden));
}

static void send_301_location(int client_fd, const char *location)
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
static void send_200_header(int client_fd, const char *mime, off_t len)
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

static int stream_file_fd(int client_fd, int fd, off_t filesize)
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

/* join directory and request path safely into out (must be PATH_MAX). returns 0 on success */
static int join_path(const char *dir, const char *req, char *out, size_t outlen)
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

void handle_client(int client_fd, const char *content_directory, bool show_ext)
{
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    char method[16] = {0}, path[1024] = {0};
    if (sscanf(buffer, "%15s %1023s", method, path) != 2)
    {
        close(client_fd);
        return;
    }

    /* Only handle GET and HEAD */
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
    {
        const char *not_impl =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Allow: GET, HEAD\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_fd, not_impl, strlen(not_impl));
        close(client_fd);
        return;
    }

    /* Basic reject for raw traversal tokens before decoding */
    if (strstr(path, ".."))
    {
        send_403(client_fd);
        close(client_fd);
        return;
    }

    /* URL-decode path in-place */
    if (url_decode(path) != 0)
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }

    /* ensure path starts with '/' */
    if (path[0] != '/')
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }

    /* canonicalize content_directory absolute path */
    char abs_content[PATH_MAX];
    if (!realpath(content_directory, abs_content))
    {
        /* can't resolve content dir -> fail closed */
        close(client_fd);
        return;
    }

    /* SHOW-EXTENSION mode */
    if (show_ext)
    {
        /* redirect root to /index.html */
        if (strcmp(path, "/") == 0)
        {
            send_301_location(client_fd, "/index.html");
            close(client_fd);
            return;
        }

        /* candidate filesystem path (literal) */
        char candidate[PATH_MAX];
        if (join_path(content_directory, path, candidate, sizeof(candidate)) != 0)
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }

        /* If candidate is a directory, try index.html inside it.
           If candidate is a file, serve it. */
        struct stat st;
        if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode))
        {
            /* ensure trailing slash in URL form; redirect to slash-form if absent */
            size_t plen = strlen(path);
            if (path[plen - 1] != '/')
            {
                char with_slash[1024];
                snprintf(with_slash, sizeof(with_slash), "%s/", path);
                send_301_location(client_fd, with_slash);
                close(client_fd);
                return;
            }
            /* append index.html and try that */
            size_t n = snprintf(candidate, sizeof(candidate), "%s%sindex.html",
                                content_directory,
                                path[0] == '/' ? path + 1 : path);
            if (n >= sizeof(candidate))
            {
                send_404(client_fd);
                close(client_fd);
                return;
            }
            if (stat(candidate, &st) != 0 || !S_ISREG(st.st_mode))
            {
                send_404(client_fd);
                close(client_fd);
                return;
            }
        }
        else if (stat(candidate, &st) != 0 || !S_ISREG(st.st_mode))
        {
            /* If literal path not found, also try appending .html (helpful if client requested /foo and
               you want to serve /foo.html in show_ext mode) */
            if (strrchr(path, '.') == NULL)
            {
                char alt[PATH_MAX];
                if (snprintf(alt, sizeof(alt), "%s%s.html", content_directory, path) < (int)sizeof(alt))
                {
                    if (stat(alt, &st) == 0 && S_ISREG(st.st_mode))
                    {
                        strncpy(candidate, alt, sizeof(candidate) - 1);
                        candidate[sizeof(candidate) - 1] = '\0';
                    }
                    else
                    {
                        send_404(client_fd);
                        close(client_fd);
                        return;
                    }
                }
                else
                {
                    send_404(client_fd);
                    close(client_fd);
                    return;
                }
            }
            else
            {
                send_404(client_fd);
                close(client_fd);
                return;
            }
        }

        /* canonicalize candidate and ensure it's under content_directory */
        char abs_candidate[PATH_MAX];
        if (!realpath(candidate, abs_candidate))
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        if (strncmp(abs_candidate, abs_content, strlen(abs_content)) != 0 ||
            (abs_candidate[strlen(abs_content)] != '/' && abs_candidate[strlen(abs_content)] != '\0'))
        {
            send_403(client_fd);
            close(client_fd);
            return;
        }

        int fd = open(abs_candidate, O_RDONLY);
        if (fd < 0)
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        if (fstat(fd, &st) != 0)
        {
            close(fd);
            close(client_fd);
            return;
        }
        off_t file_size = st.st_size;

        const char *mime = get_mime_type(path);
        if (strcmp(method, "HEAD") != 0)
            send_200_header(client_fd, mime, file_size);
        else
        {
            /* HEAD should send headers only */
            send_200_header(client_fd, mime, file_size);
            close(fd);
            close(client_fd);
            return;
        }

        if (stream_file_fd(client_fd, fd, file_size) != 0)
        {
            close(fd);
            close(client_fd);
            return;
        }
        close(fd);
        close(client_fd);
        return;
    }

    /* HIDE-EXTENSION mode */
    /* If request is "/": serve /index.html (no redirect) */
    if (strcmp(path, "/") == 0)
    {
        char cand[PATH_MAX];
        if (join_path(content_directory, "/index.html", cand, sizeof(cand)) != 0)
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        struct stat st;
        if (stat(cand, &st) != 0 || !S_ISREG(st.st_mode))
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        char abs_cand[PATH_MAX];
        if (!realpath(cand, abs_cand))
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        char abs_content2[PATH_MAX];
        if (!realpath(content_directory, abs_content2))
        {
            close(client_fd);
            return;
        }
        if (strncmp(abs_cand, abs_content2, strlen(abs_content2)) != 0)
        {
            send_403(client_fd);
            close(client_fd);
            return;
        }

        int fd = open(abs_cand, O_RDONLY);
        if (fd < 0)
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
        if (fstat(fd, &st) != 0)
        {
            close(fd);
            close(client_fd);
            return;
        }
        off_t file_size = st.st_size;
        const char *mime = get_mime_type("/index.html");
        if (strcmp(method, "HEAD") != 0)
            send_200_header(client_fd, mime, file_size);
        else
        {
            send_200_header(client_fd, mime, file_size);
            close(fd);
            close(client_fd);
            return;
        }
        if (stream_file_fd(client_fd, fd, file_size) != 0)
        {
            close(fd);
            close(client_fd);
            return;
        }
        close(fd);
        close(client_fd);
        return;
    }

    /* Normalize path: strip trailing slash (but remember if it had slash) */
    bool had_trailing_slash = false;
    size_t plen = strlen(path);
    if (plen > 1 && path[plen - 1] == '/')
    {
        had_trailing_slash = true;
        path[plen - 1] = '\0';
    }

    /* If request explicitly ends with .html, check if clean path (with trailing slash) should redirect.
       Only redirect when the clean target exists as directory/index.html (preserves previous intent). */
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".html") == 0)
    {
        /* build filesystem path to requested .html */
        char requested_fs[PATH_MAX];
        if (join_path(content_directory, path, requested_fs, sizeof(requested_fs)) == 0)
        {
            struct stat rst;
            if (stat(requested_fs, &rst) == 0 && S_ISREG(rst.st_mode))
            {
                /* compute clean path (remove .html) and test for directory/index.html */
                size_t base_len = (size_t)(ext - path); /* length of "/name" */
                char clean_path[1024];
                if (base_len == 0)
                    strncpy(clean_path, "/", sizeof(clean_path));
                else
                {
                    size_t copy_len = base_len;
                    if (copy_len >= sizeof(clean_path) - 2)
                        copy_len = sizeof(clean_path) - 2;
                    memcpy(clean_path, path, copy_len);
                    clean_path[copy_len] = '\0';
                    /* ensure trailing slash on clean path used in redirect */
                    size_t rl = strlen(clean_path);
                    if (rl == 0 || clean_path[rl - 1] != '/')
                    {
                        if (rl + 1 < sizeof(clean_path))
                            strcat(clean_path, "/");
                    }
                }

                /* candidate = content_directory + clean_path + "index.html" */
                char candidate_fs[PATH_MAX];
                if (snprintf(candidate_fs, sizeof(candidate_fs), "%s%sindex.html", content_directory,
                             clean_path[0] == '/' ? clean_path + 1 : clean_path) < (int)sizeof(candidate_fs))
                {
                    struct stat cst;
                    if (stat(candidate_fs, &cst) == 0 && S_ISREG(cst.st_mode))
                    {
                        /* redirect to clean_path (with trailing slash) */
                        send_301_location(client_fd, clean_path);
                        close(client_fd);
                        return;
                    }
                }
                /* if candidate doesn't exist, fall through to serve requested .html */
            }
        }
    }

    /* If request has no extension, try appending .html */
    char resolved_req[PATH_MAX];
    if (strrchr(path, '.') == NULL)
    {
        if (snprintf(resolved_req, sizeof(resolved_req), "%s.html", path) >= (int)sizeof(resolved_req))
        {
            send_404(client_fd);
            close(client_fd);
            return;
        }
    }
    else
    {
        strncpy(resolved_req, path, sizeof(resolved_req) - 1);
        resolved_req[sizeof(resolved_req) - 1] = '\0';
    }

    /* Build candidate FS path */
    char candidate_fs[PATH_MAX];
    if (join_path(content_directory, resolved_req, candidate_fs, sizeof(candidate_fs)) != 0)
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }

    /* canonicalize candidate and ensure it's inside content_directory */
    char abs_candidate[PATH_MAX];
    if (!realpath(candidate_fs, abs_candidate))
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }
    if (strncmp(abs_candidate, abs_content, strlen(abs_content)) != 0 ||
        (abs_candidate[strlen(abs_content)] != '/' && abs_candidate[strlen(abs_content)] != '\0'))
    {
        send_403(client_fd);
        close(client_fd);
        return;
    }

    struct stat st;
    if (stat(abs_candidate, &st) != 0 || !S_ISREG(st.st_mode))
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }

    int fd = open(abs_candidate, O_RDONLY);
    if (fd < 0)
    {
        send_404(client_fd);
        close(client_fd);
        return;
    }
    off_t file_size = st.st_size;

    const char *mime = get_mime_type(resolved_req);
    if (strcmp(method, "HEAD") != 0)
        send_200_header(client_fd, mime, file_size);
    else
    {
        send_200_header(client_fd, mime, file_size);
        close(fd);
        close(client_fd);
        return;
    }

    if (stream_file_fd(client_fd, fd, file_size) != 0)
    {
        close(fd);
        close(client_fd);
        return;
    }
    close(fd);
    close(client_fd);
}

void run_server_loop(int server_fd, const char *content_directory, const bool show_ext)
{
    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        handle_client(client_fd, content_directory, show_ext);
        printf("Loop Running\n");
    }
}