#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define READ_END 0
#define WRITE_END 1

// Minimal helper to safe strdup when NULL allowed
static char *safe_strdup(const char *s)
{
    if (!s)
        return NULL;
    char *d = strdup(s);
    return d;
}

// Write exactly len bytes to fd; return 0 on success or -1 on error
static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    size_t left = len;
    while (left > 0)
    {
        ssize_t w = write(fd, p, left);
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return 0;
}

// Read a line (ending with \n) from fd into buf up to bufsz-1, returns bytes read or -1
static ssize_t read_line_fd(int fd, char *buf, size_t bufsz)
{
    size_t idx = 0;
    while (idx + 1 < bufsz)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0)
        { // EOF
            if (idx == 0)
                return 0;
            break;
        }
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf[idx++] = c;
        if (c == '\n')
            break;
    }
    buf[idx] = '\0';
    return (ssize_t)idx;
}

// Parse "Status: NNN Reason" header returned by CGI and format HTTP status line
// Returns 0 if status provided, -1 otherwise. On success sets out_status_line which must be freed.
static int parse_cgi_status(const char *header_line, char **out_status_line)
{
    if (!header_line)
        return -1;
    // header_line includes newline; copy and trim
    char tmp[256];
    strncpy(tmp, header_line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    // Look for "Status:"
    const char *p = tmp;
    while (*p && (*p == ' ' || *p == '\t'))
        ++p;
    if (strncasecmp(p, "Status:", 7) != 0)
        return -1;
    p += 7;
    while (*p == ' ' || *p == '\t')
        ++p;
    // Now p should start with "NNN"
    int code = 0;
    if (sscanf(p, "%d", &code) != 1)
        return -1;
    // Find rest of line after code for reason phrase
    const char *rest = p;
    while (*rest && (*rest >= '0' && *rest <= '9'))
        ++rest;
    while (*rest == ' ' || *rest == '\t')
        ++rest;
    // Build HTTP status line
    char status_line[128];
    if (*rest)
        snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", code, rest);
    else
        snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d OK\r\n", code);
    *out_status_line = strdup(status_line);
    return 0;
}

// Main handler
// script_path must be the absolute or relative filesystem path to the .php file
// client_fd is the connected socket to write the HTTP response
// method is "GET" or "POST" etc.
// query_string may be NULL or empty
// content_type may be NULL
// body points to request body for POST and body_len is its length (0 if none)
void handle_php_request(const char *script_path,
                        int client_fd,
                        const char *method,
                        const char *query_string,
                        const char *content_type,
                        const char *remote_addr,
                        const char *server_name,
                        const char *server_software,
                        const char *body,
                        size_t body_len)
{
    if (!script_path || !method || client_fd < 0)
        return;

    int inpipe[2];  // parent -> child (child stdin)
    int outpipe[2]; // child stdout -> parent

    if (pipe(inpipe) < 0 || pipe(outpipe) < 0)
    {
        perror("pipe");
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(inpipe[READ_END]);
        close(inpipe[WRITE_END]);
        close(outpipe[READ_END]);
        close(outpipe[WRITE_END]);
        return;
    }

    if (pid == 0)
    {
        // Child: set up stdin/stdout, close unused fds
        dup2(inpipe[READ_END], STDIN_FILENO);
        dup2(outpipe[WRITE_END], STDOUT_FILENO);
        // Close pipe ends not used
        close(inpipe[READ_END]);
        close(inpipe[WRITE_END]);
        close(outpipe[READ_END]);
        close(outpipe[WRITE_END]);

        // Set CGI environment variables
        if (setenv("GATEWAY_INTERFACE", "CGI/1.1", 1) != 0)
        { /* ignore */
        }
        setenv("REQUEST_METHOD", method, 1);
        if (query_string)
            setenv("QUERY_STRING", query_string, 1);
        setenv("SCRIPT_FILENAME", script_path, 1);
        // SCRIPT_NAME is often the URI path; if unavailable, set to script_path
        setenv("SCRIPT_NAME", script_path, 1);
        if (remote_addr)
            setenv("REMOTE_ADDR", remote_addr, 1);
        if (server_name)
            setenv("SERVER_NAME", server_name, 1);
        if (server_software)
            setenv("SERVER_SOFTWARE", server_software, 1);
        if (content_type)
            setenv("CONTENT_TYPE", content_type, 1);
        if (body_len > 0)
        {
            char numbuf[32];
            snprintf(numbuf, sizeof(numbuf), "%zu", body_len);
            setenv("CONTENT_LENGTH", numbuf, 1);
        }
        else
        {
            unsetenv("CONTENT_LENGTH");
        }

        // Execute php-cgi. Use execlp so PATH is used.
        execlp("php-cgi", "php-cgi", NULL);

        // If execlp fails
        perror("execlp php-cgi");
        _exit(1);
    }

    // Parent: close unused ends
    close(inpipe[READ_END]);
    close(outpipe[WRITE_END]);

    // If there's a request body (POST), write it into child stdin, then close child's stdin
    if (body_len > 0 && body != NULL)
    {
        if (write_all(inpipe[WRITE_END], body, body_len) != 0)
        {
            // If write fails, keep going but close pipe
        }
    }
    // Close write end to signal EOF to php-cgi
    close(inpipe[WRITE_END]);

    // Read child's stdout, which contains CGI headers then body.
    // We need to parse headers until an empty line, then forward a proper HTTP response.
    // Strategy:
    // 1) Read line by line until blank line ("\r\n" or "\n")
    // 2) Collect headers, detect Status header
    // 3) Write HTTP status line and headers to client, then stream remainder

    char line[4096];
    int got_blank = 0;
    char *status_line = NULL;
// Temporary buffer to store other headers
#define MAX_HEADERS 64
    char *headers[MAX_HEADERS];
    int header_count = 0;

    // Read header lines
    while (1)
    {
        ssize_t n = read_line_fd(outpipe[READ_END], line, sizeof(line));
        if (n < 0)
        {
            // read error
            break;
        }
        if (n == 0)
        {
            // EOF reached before headers finished
            break;
        }
        // Trim CRLF end
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\r' || line[L - 1] == '\n'))
        {
            line[--L] = '\0';
        }
        if (L == 0)
        {
            got_blank = 1;
            break;
        }
        // Handle Status header specially
        if (strncasecmp(line, "Status:", 7) == 0)
        {
            parse_cgi_status(line, &status_line);
            continue;
        }
        // Store header
        if (header_count < MAX_HEADERS)
        {
            headers[header_count++] = strdup(line);
        }
    }

    // If php-cgi did not return a Status header, default to 200
    if (!status_line)
    {
        status_line = strdup("HTTP/1.1 200 OK\r\n");
    }

    // Send status line
    if (write_all(client_fd, status_line, strlen(status_line)) != 0)
    {
        // cannot write to client, cleanup and return
        free(status_line);
        for (int i = 0; i < header_count; ++i)
            free(headers[i]);
        close(outpipe[READ_END]);
        waitpid(pid, NULL, 0);
        return;
    }
    free(status_line);

    // Ensure Content-Type exists among headers. If php-cgi gave headers, forward them.
    int has_content_type = 0;
    for (int i = 0; i < header_count; ++i)
    {
        if (strncasecmp(headers[i], "Content-Type:", 13) == 0)
            has_content_type = 1;
        // Write header line + CRLF
        char tmpbuf[4096];
        snprintf(tmpbuf, sizeof(tmpbuf), "%s\r\n", headers[i]);
        write_all(client_fd, tmpbuf, strlen(tmpbuf));
        free(headers[i]);
    }

    // If php-cgi produced no Content-Type, add a default
    if (!has_content_type)
    {
        const char *fallback = "Content-Type: text/html; charset=UTF-8\r\n";
        write_all(client_fd, fallback, strlen(fallback));
    }

    // End of headers
    write_all(client_fd, "\r\n", 2);

    // Now stream remaining output from php-cgi to client until EOF
    char buf[8192];
    ssize_t r;
    while ((r = read(outpipe[READ_END], buf, sizeof(buf))) > 0)
    {
        if (write_all(client_fd, buf, (size_t)r) != 0)
            break;
    }

    close(outpipe[READ_END]);

    // Wait for child to exit
    int status = 0;
    waitpid(pid, &status, 0);
}