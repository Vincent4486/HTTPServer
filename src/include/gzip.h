#ifndef GZIP_H
#define GZIP_H

#include <stddef.h>

/* Compress data using gzip
   Returns malloc'd compressed buffer on success, NULL on failure
   *out_size is set to compressed size */
char *gzip_compress(const char *data, size_t data_size, size_t *out_size);

/* Free gzip-compressed buffer */
void gzip_free(char *compressed);

#endif
