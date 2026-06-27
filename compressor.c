#include "compressor.h"
#include <stdlib.h>
#include <string.h>

#define WINDOW_SIZE 32768
#define MIN_MATCH   3
#define MAX_MATCH   258
#define HASH_SIZE   32768

static inline uint16_t get_hash(const uint8_t *data) {
    return ((data[0] << 8) ^ (data[1] << 3) ^ data[2]) & (HASH_SIZE - 1);
}

uint32_t compress_block(const uint8_t *src, uint32_t src_size, uint8_t *dst, int *is_compressed) {
    uint32_t src_idx = 0, dst_idx = 0;

    uint8_t *tmp = (uint8_t *)malloc(src_size + (src_size / 8) + 32);
    int32_t *hash_head = (int32_t *)malloc(HASH_SIZE * sizeof(int32_t));
    int32_t *hash_prev = (int32_t *)malloc(CHUNK_SIZE * sizeof(int32_t));

    if (!tmp || !hash_head || !hash_prev) {
        if (tmp) free(tmp);
        if (hash_head) free(hash_head);
        if (hash_prev) free(hash_prev);
        *is_compressed = 0;
        return 0;
    }

    memset(hash_head, -1, HASH_SIZE * sizeof(int32_t));
    memset(hash_prev, -1, CHUNK_SIZE * sizeof(int32_t));

    while (src_idx < src_size) {
        uint32_t flag_pos = dst_idx++;
        tmp[flag_pos] = 0;

        for (int bit = 0; bit < 8; bit++) {
            if (src_idx >= src_size) break;

            uint32_t best_offset = 0;
            uint32_t best_len = 0;

            if (src_size - src_idx >= MIN_MATCH) {
                uint16_t hash = get_hash(&src[src_idx]);
                int32_t match_idx = hash_head[hash];

                int chain_len = 0;
                while (match_idx != -1 && chain_len < 256) {
                    uint32_t offset = src_idx - match_idx;
                    if (offset >= WINDOW_SIZE) break;

                    uint32_t len = 0;
                    while (src_idx + len < src_size && src[match_idx + len] == src[src_idx + len] && len < MAX_MATCH) {
                        len++;
                    }

                    if (len > best_len && len >= MIN_MATCH) {
                        best_len = len;
                        best_offset = offset;
                        if (len == MAX_MATCH) break;
                    }

                    match_idx = hash_prev[match_idx];
                    chain_len++;
                }

                hash_prev[src_idx] = hash_head[hash];
                hash_head[hash] = (int32_t)src_idx;
            }

            if (best_len >= MIN_MATCH) {
                tmp[flag_pos] |= (1 << bit);

                uint32_t match_data = (best_offset & 0x7FFF) | (((best_len - MIN_MATCH) & 0x1FF) << 15);

                tmp[dst_idx++] = (uint8_t)(match_data & 0xFF);
                tmp[dst_idx++] = (uint8_t)((match_data >> 8) & 0xFF);
                tmp[dst_idx++] = (uint8_t)((match_data >> 16) & 0xFF);

                for (uint32_t i = 1; i < best_len && (src_idx + i + 2) < src_size; i++) {
                    uint16_t h = get_hash(&src[src_idx + i]);
                    hash_prev[src_idx + i] = hash_head[h];
                    hash_head[h] = (int32_t)(src_idx + i);
                }
                src_idx += best_len;
            } else {
                tmp[dst_idx++] = src[src_idx];
                src_idx++;
            }
        }
    }

    free(hash_head);
    free(hash_prev);

    if (dst_idx >= src_size) {
        memcpy(dst, src, src_size);
        *is_compressed = 0;
        free(tmp);
        return src_size;
    }

    memcpy(dst, tmp, dst_idx);
    *is_compressed = 1;
    free(tmp);
    return dst_idx;
}

void decompress_block(const uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dest_size) {
    uint32_t src_idx = 0, dst_idx = 0;

    while (src_idx < src_size && dst_idx < dest_size) {
        uint8_t flags = src[src_idx++];

        for (int bit = 0; bit < 8; bit++) {
            if (dst_idx >= dest_size) break;

            if (flags & (1 << bit)) {
                if (src_idx + 2 >= src_size) return;

                uint8_t b1 = src[src_idx++];
                uint8_t b2 = src[src_idx++];
                uint8_t b3 = src[src_idx++];

                uint32_t match_data = (uint32_t)(b1 | (b2 << 8) | (b3 << 16));

                uint32_t offset = match_data & 0x7FFF;
                uint32_t len = (match_data >> 15) + MIN_MATCH;

                if (offset == 0 || offset > dst_idx || dst_idx + len > dest_size) {
                    return;
                }

                uint32_t from = dst_idx - offset;
                for (uint32_t i = 0; i < len; i++) {
                    dst[dst_idx++] = dst[from + i];
                }
            } else {
                if (src_idx >= src_size) break;
                dst[dst_idx++] = src[src_idx++];
            }
        }
    }
}
