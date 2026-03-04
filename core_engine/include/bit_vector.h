#ifndef BIT_VECTOR_H
#define BIT_VECTOR_H

/*
 * bit_vector.h — ArborFlow Gatekeeper Layer 1
 * --------------------------------------------
 * A memory-efficient boolean array using bitwise operations.
 * Each IP is mapped to a single bit, giving O(1) lookup/insert/delete.
 *
 * Memory cost for 2^16 entries (bottom-tier cluster): 8 KB
 * Memory cost for full 2^32 space (if used directly): 512 MB — avoid this.
 * Use the two-tier GatekeeperVEB for 32-bit IPs instead.
 */

#include <stdint.h>
#include <stdlib.h>

/* Number of bits per word (using 64-bit words for efficiency) */
#define BV_WORD_SIZE   64
#define BV_WORD_SHIFT  6                        /* log2(64) */
#define BV_WORD_MASK   (BV_WORD_SIZE - 1)       /* 0x3F     */

typedef struct {
    uint64_t *words;    /* The underlying bit array               */
    uint32_t  capacity; /* Total number of bits this vector holds */
    uint32_t  num_words;/* Number of 64-bit words allocated       */
} BitVector;

/*
 * bv_create  — Allocate a BitVector with `capacity` bits, all set to 0.
 *              Returns NULL on allocation failure.
 */
BitVector *bv_create(uint32_t capacity);

/*
 * bv_destroy — Free all memory associated with a BitVector.
 */
void bv_destroy(BitVector *bv);

/*
 * bv_set     — Mark bit `index` as 1 (IP is blacklisted).
 */
void bv_set(BitVector *bv, uint32_t index);

/*
 * bv_clear   — Mark bit `index` as 0 (remove from blacklist).
 */
void bv_clear(BitVector *bv, uint32_t index);

/*
 * bv_contains — Returns 1 if bit `index` is set, 0 otherwise. O(1).
 */
int bv_contains(const BitVector *bv, uint32_t index);

/*
 * bv_reset   — Set all bits to 0 (clear the entire blacklist).
 */
void bv_reset(BitVector *bv);

#endif /* BIT_VECTOR_H */
