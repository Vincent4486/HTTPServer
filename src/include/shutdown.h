#ifndef SHUTDOWN_H
#define SHUTDOWN_H

#include <sig-atomic.h>

/* Initialize signal handlers for graceful shutdown */
void init_signal_handlers(void);

/* Check if shutdown signal was received */
int is_shutdown_requested(void);

#endif
