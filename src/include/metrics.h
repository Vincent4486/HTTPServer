#ifndef METRICS_H
#define METRICS_H

#include <time.h>

typedef struct
{
    unsigned long total_requests;
    unsigned long total_bytes;
    double min_response_time;
    double max_response_time;
    double avg_response_time;
    time_t start_time;
    unsigned long current_memory_bytes;
    unsigned long peak_memory_bytes;
    double total_cpu_time_ms;
} metrics_t;

/* Initialize metrics */
void metrics_init(void);

/* Record request (pass response time in milliseconds) */
void metrics_record_request(size_t bytes_sent, double response_time_ms);

/* Get current metrics snapshot */
metrics_t metrics_get(void);

/* Get uptime in seconds */
unsigned long metrics_get_uptime(void);

/* Update memory usage stats (called during request processing) */
void metrics_update_memory(void);

#endif
