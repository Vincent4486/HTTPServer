#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "include/gzip.h"

/* Compress data using gzip
   Returns malloc'd compressed buffer on success, NULL on failure
   *out_size is set to compressed size */
char *gzip_compress(const char *data, size_t data_size, size_t *out_size)
{
    if (!data || data_size == 0 || !out_size)
        return NULL;

    /* Estimate compressed size (worst case: input size + some overhead) */
    uLongf compressed_size = compressBound(data_size);
    char *compressed = malloc(compressed_size);
    if (!compressed)
        return NULL;

    /* Compress using zlib with gzip format */
    int ret = compress2((unsigned char *)compressed, &compressed_size,
                        (const unsigned char *)data, data_size, Z_DEFAULT_COMPRESSION);

    if (ret != Z_OK)
    {
        free(compressed);
        return NULL;
    }

    *out_size = compressed_size;
    return compressed;
}

/* Free gzip-compressed buffer */
void gzip_free(char *compressed)
{
    free(compressed);
}
