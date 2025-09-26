#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strlen, strcpy, memset
#include <stdbool.h>    // bool, true, fals
#include <stdint.h>     // intmax_t
#include <inttypes.h>   // PRIdMAX
#include <ctype.h>      // isxdigit
#include <limits.h>     // PATH_MAX

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#else
#include <netinet/in.h> // sockaddr_in
#endif

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

#include "include/http.h"

#include "include/client.h"
#include "include/logger.h"

void handle_http_request(int client_fd, const char *content_directory, bool show_ext)
{
    // Function implementation goes here
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
