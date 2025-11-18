#ifndef ACCESS_LOG_H
#define ACCESS_LOG_H

/* Initialize access logging system */
void access_log_init(const char *log_file);

/* Log a single HTTP request/response in Apache combined format */
void access_log_request(
    const char *client_ip,
    const char *method,
    const char *path,
    const char *protocol,
    int status_code,
    long bytes_sent,
    const char *referer,
    const char *user_agent);

/* Close and cleanup access logging */
void access_log_close(void);

#endif
