#ifndef VEB_TREE_H
#define VEB_TREE_H

/*
 * veb_tree.h — ArborFlow Gatekeeper Layer 2
 * ------------------------------------------
 * Two-tier RS-vEB (Real-Space Van Emde Boas) Tree for 32-bit IPv4 space.
 *
 * Standard vEB over u=2^32 is impractical (gigabytes of pointer arrays).
 * This implementation splits each IP into two 16-bit halves:
 *
 *   top  = ip >> 16        → indexes into a hash map (cluster selector)
 *   bot  = ip & 0xFFFF     → indexes into a 64KB BitVector (cluster content)
 *
 * Memory is allocated per-cluster on demand. An empty cluster costs nothing.
 * A populated cluster costs one 8KB BitVector (2^16 bits / 8).
 *
 * Complexity:
 *   member / insert / delete — O(1) amortized (hash lookup + bit op)
 *   Worst-case space          — O(n) where n = distinct top-16 prefixes
 *
 * This is faithful to the vEB spirit (hierarchical universe decomposition)
 * while being safe and practical for production use in C.
 */

#include <stdint.h>
#include <stdlib.h>
#include "../include/bit_vector.h"

/* ------------------------------------------------------------------ */
/*  Internal hash map: maps uint16_t cluster index → BitVector*       */
/* ------------------------------------------------------------------ */

#define VEB_HM_CAPACITY  1024   /* Must be power of 2 */
#define VEB_HM_MASK      (VEB_HM_CAPACITY - 1)

typedef struct VebHMEntry {
    uint16_t           key;
    BitVector         *cluster;
    struct VebHMEntry *next;    /* Chaining for collision resolution */
} VebHMEntry;

typedef struct {
    VebHMEntry *buckets[VEB_HM_CAPACITY];
    uint32_t    size;           /* Number of active clusters */
} VebHashMap;

/* ------------------------------------------------------------------ */
/*  Two-tier RS-vEB structure                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    VebHashMap *top;            /* top-16 → BitVector* for bottom-16  */
    int         min;            /* Global min stored IP (-1 if empty) */
    int         max;            /* Global max stored IP (-1 if empty) */
    uint32_t    count;          /* Total number of IPs stored         */
} VebTree;

/*
 * veb_create  — Allocate and initialise an empty VebTree.
 *               Returns NULL on failure.
 */
VebTree *veb_create(void);

/*
 * veb_destroy — Free all memory used by the tree including all clusters.
 */
void veb_destroy(VebTree *veb);

/*
 * veb_insert  — Insert a 32-bit IP into the tree.
 */
void veb_insert(VebTree *veb, uint32_t ip);

/*
 * veb_delete  — Remove a 32-bit IP from the tree.
 */
void veb_delete(VebTree *veb, uint32_t ip);

/*
 * veb_member  — Returns 1 if `ip` is in the tree, 0 otherwise. O(1) amortized.
 */
int veb_member(const VebTree *veb, uint32_t ip);

/*
 * veb_is_empty — Returns 1 if the tree holds no IPs.
 */
int veb_is_empty(const VebTree *veb);

#endif /* VEB_TREE_H */
