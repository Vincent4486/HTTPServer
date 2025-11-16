#ifndef LOGGER_H
#define LOGGER_H

void log_info(const char *message);
void log_error(const char *message);
/* Log an error with a numeric code. Code is a small integer corresponding to entries
	in err-code-list.md. The message is a printf-style format string. */
void log_error_code(int code, const char *fmt, ...);

#endif // LOGGER_H