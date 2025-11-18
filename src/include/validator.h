#ifndef VALIDATOR_H
#define VALIDATOR_H

/* Validate config.json on startup
   Returns 1 if valid, 0 if invalid (will exit on error) */
int validate_config(void);

#endif
