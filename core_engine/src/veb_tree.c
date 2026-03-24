#include "../include/veb_tree.h"
#include <string.h>
#include <stdio.h>

/*
 * veb_tree.c — ArborFlow Gatekeeper Layer 2
 * ------------------------------------------
 * Two-tier RS-vEB implementation.
 *
 * Each 32-bit IP is split:
 *   top (uint16_t) = ip >> 16
 *   bot (uint16_t) = ip & 0xFFFF
 *
 * Lookups: hash(top) → BitVector, then bv_contains(bot).
 * Inserts: create cluster BitVector on first use, then bv_set(bot).
 */

/* ------------------------------------------------------------------ */
/*  Internal hash map implementation                                   */
/* ------------------------------------------------------------------ */

static uint32_t hm_hash(uint16_t key) {
    /* FNV-1a inspired mix for 16-bit key */
    uint32_t h = (uint32_t)key;
    h ^= h >> 8;
    h *= 0x9e3779b9u;
    h ^= h >> 16;
    return h & VEB_HM_MASK;
}

static VebHashMap *hm_create(void) {
    VebHashMap *hm = (VebHashMap *)calloc(1, sizeof(VebHashMap));
    if (!hm) {
        fprintf(stderr, "[VebTree] calloc failed for hash map\n");
    }
    return hm;
}

/* Returns the BitVector* for `key`, or NULL if that cluster doesn't exist */
static BitVector *hm_get(const VebHashMap *hm, uint16_t key) {
    uint32_t idx = hm_hash(key);
    VebHMEntry *e = hm->buckets[idx];
    while (e) {
        if (e->key == key) return e->cluster;
        e = e->next;
    }
    return NULL;
}

/*
 * hm_get_or_create — Return the cluster BitVector for `key`.
 * If no cluster exists yet, allocate one (2^16 bits = 8 KB).
 */
static BitVector *hm_get_or_create(VebHashMap *hm, uint16_t key) {
    uint32_t idx = hm_hash(key);
    VebHMEntry *e = hm->buckets[idx];

    /* Search for existing entry */
    while (e) {
        if (e->key == key) return e->cluster;
        e = e->next;
    }

    /* Not found — allocate a new cluster */
    BitVector *bv = bv_create(1 << 16);  /* 65536 bits = 8 KB */
    if (!bv) return NULL;

    VebHMEntry *entry = (VebHMEntry *)malloc(sizeof(VebHMEntry));
    if (!entry) {
        bv_destroy(bv);
        return NULL;
    }

    entry->key     = key;
    entry->cluster = bv;
    entry->next    = hm->buckets[idx];  /* Prepend (O(1)) */
    hm->buckets[idx] = entry;
    hm->size++;

    return bv;
}

/* Remove a cluster entry. Does NOT destroy the BitVector (caller's job). */
static void hm_remove(VebHashMap *hm, uint16_t key) {
    uint32_t    idx  = hm_hash(key);
    VebHMEntry *prev = NULL;
    VebHMEntry *e    = hm->buckets[idx];

    while (e) {
        if (e->key == key) {
            if (prev) prev->next = e->next;
            else       hm->buckets[idx] = e->next;
            bv_destroy(e->cluster);
            free(e);
            hm->size--;
            return;
        }
        prev = e;
        e    = e->next;
    }
}

static void hm_destroy(VebHashMap *hm) {
    if (!hm) return;
    for (int i = 0; i < VEB_HM_CAPACITY; i++) {
        VebHMEntry *e = hm->buckets[i];
        while (e) {
            VebHMEntry *next = e->next;
            bv_destroy(e->cluster);
            free(e);
            e = next;
        }
    }
    free(hm);
}

/* ------------------------------------------------------------------ */
/*  VebTree public API                                                 */
/* ------------------------------------------------------------------ */

VebTree *veb_create(void) {
    VebTree *veb = (VebTree *)malloc(sizeof(VebTree));
    if (!veb) {
        fprintf(stderr, "[VebTree] malloc failed for VebTree struct\n");
        return NULL;
    }

    veb->top   = hm_create();
    veb->min   = -1;
    veb->max   = -1;
    veb->count = 0;

    if (!veb->top) {
        free(veb);
        return NULL;
    }

    return veb;
}

void veb_destroy(VebTree *veb) {
    if (!veb) return;
    hm_destroy(veb->top);
    free(veb);
}

void veb_insert(VebTree *veb, uint32_t ip) {
    if (!veb) return;

    uint16_t top = (uint16_t)(ip >> 16);
    uint16_t bot = (uint16_t)(ip & 0xFFFF);

    BitVector *cluster = hm_get_or_create(veb->top, top);
    if (!cluster) return;

    /* Only update count and min/max if not already present */
    if (!bv_contains(cluster, bot)) {
        bv_set(cluster, bot);
        veb->count++;

        if (veb->min == -1 || (int32_t)ip < veb->min) veb->min = (int32_t)ip;
        if (veb->max == -1 || (int32_t)ip > veb->max) veb->max = (int32_t)ip;
    }
}

void veb_delete(VebTree *veb, uint32_t ip) {
    if (!veb) return;

    uint16_t top = (uint16_t)(ip >> 16);
    uint16_t bot = (uint16_t)(ip & 0xFFFF);

    BitVector *cluster = hm_get(veb->top, top);
    if (!cluster) return;
    if (!bv_contains(cluster, bot)) return;

    bv_clear(cluster, bot);
    veb->count--;

    /*
     * Lazy cleanup: if the cluster is now empty, free it from the map.
     * We don't scan for new min/max here — if you need exact min/max
     * after deletes, a full scan or secondary structure is required.
     * For Gatekeeper use (membership queries only), this is sufficient.
     */
    /* Check if cluster is now empty by scanning its words */
    int empty = 1;
    for (uint32_t w = 0; w < cluster->num_words; w++) {
        if (cluster->words[w] != 0) { empty = 0; break; }
    }
    if (empty) {
        hm_remove(veb->top, top);  /* Also destroys the BitVector */
    }

    if (veb->count == 0) {
        veb->min = -1;
        veb->max = -1;
    }
}

int veb_member(const VebTree *veb, uint32_t ip) {
    if (!veb || veb->count == 0) return 0;

    uint16_t top = (uint16_t)(ip >> 16);
    uint16_t bot = (uint16_t)(ip & 0xFFFF);

    BitVector *cluster = hm_get(veb->top, top);
    if (!cluster) return 0;

    return bv_contains(cluster, bot);
}

int veb_is_empty(const VebTree *veb) {
    return (!veb || veb->count == 0);
}
