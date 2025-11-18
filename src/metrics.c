#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "include/metrics.h"

static struct
{
    unsigned long total_requests;
    unsigned long total_bytes;
    double total_response_time;
    double min_response_time;
    double max_response_time;
    time_t start_time;
    pthread_mutex_t lock;
} metrics = {
    .total_requests = 0,
    .total_bytes = 0,
    .total_response_time = 0.0,
    .min_response_time = 1e9,
    .max_response_time = 0.0,
    .start_time = 0};

void metrics_init(void)
{
    pthread_mutex_init(&metrics.lock, NULL);
    metrics.start_time = time(NULL);
}

void metrics_record_request(size_t bytes_sent, double response_time_ms)
{
    pthread_mutex_lock(&metrics.lock);

    metrics.total_requests++;
    metrics.total_bytes += bytes_sent;
    metrics.total_response_time += response_time_ms;

    if (response_time_ms < metrics.min_response_time)
        metrics.min_response_time = response_time_ms;

    if (response_time_ms > metrics.max_response_time)
        metrics.max_response_time = response_time_ms;

    pthread_mutex_unlock(&metrics.lock);
}

metrics_t metrics_get(void)
{
    metrics_t snapshot;
    pthread_mutex_lock(&metrics.lock);

    snapshot.total_requests = metrics.total_requests;
    snapshot.total_bytes = metrics.total_bytes;
    snapshot.min_response_time = (metrics.total_requests > 0) ? metrics.min_response_time : 0.0;
    snapshot.max_response_time = metrics.max_response_time;
    snapshot.avg_response_time = (metrics.total_requests > 0) ? (metrics.total_response_time / metrics.total_requests) : 0.0;
    snapshot.start_time = metrics.start_time;

    pthread_mutex_unlock(&metrics.lock);
    return snapshot;
}

unsigned long metrics_get_uptime(void)
{
    return (unsigned long)(time(NULL) - metrics.start_time);
}
