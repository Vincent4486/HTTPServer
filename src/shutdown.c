#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdatomic.h>

#include "include/shutdown.h"
#include "include/logger.h"

static _Atomic(int) shutdown_requested = 0;

static void sigterm_handler(int sig)
{
    (void)sig; /* Avoid unused parameter warning */
    atomic_store(&shutdown_requested, 1);
}

static void sigint_handler(int sig)
{
    (void)sig;
    atomic_store(&shutdown_requested, 1);
}

void init_signal_handlers(void)
{
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigint_handler);
    log_info("Signal handlers initialized (SIGTERM, SIGINT)");
}

int is_shutdown_requested(void)
{
    return atomic_load(&shutdown_requested);
}
