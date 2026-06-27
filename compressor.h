#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <stdint.h>

#define CHUNK_SIZE (1024 * 1024)

uint32_t compress_block(const uint8_t *src, uint32_t src_size, uint8_t *dst, int *is_compressed);
void decompress_block(const uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dest_size);

#endif
