#include "../include/gatekeeper.h"
#include <stdio.h>
#include <string.h>

/*
 * gatekeeper.c — ArborFlow Filtering Gatekeeper
 * -----------------------------------------------
 * Two-layer IP filtering engine:
 *   Layer 1 — BitVector  (static blacklist,  O(1))
 *   Layer 2 — RS-vEB Tree (dynamic watchlist, O(1) amortized)
 */

int gk_init(Gatekeeper *gk) {
    if (!gk) return -1;

    memset(gk, 0, sizeof(Gatekeeper));

    /*
     * Layer 1 BitVector covers the full 16-bit top prefix space.
     * We store the FULL 32-bit IPs here using a flat 2^32 bit array
     * split into 512MB... no. We use the same two-tier approach as vEB:
     * the BitVector here covers only the bottom 16 bits (65536 entries)
     * for a single top-16 bucket. For multi-prefix blacklisting, the
     * gk_load_blacklist function uses the VebTree's internal structure.
     *
     * For the blacklist, we use a DEDICATED flat BitVector over 2^16 space
     * (top-half index only) as a cheap first gate, PLUS the vEB for full
     * 32-bit precision. This adds a near-zero cost prefix check.
     *
     * Simpler design choice for clarity: the blacklist IS a VebTree.
     * Layer 1 gives O(1) via its hash+bitvector internally.
     * The "BitVector" struct is used inside the VebTree already.
     * We expose it as a VebTree here with a semantic blacklist role.
     */

    /* Layer 1: static blacklist (RS-vEB — O(1) amortised, memory-safe) */
    gk->blacklist = bv_create(1 << 16);
    if (!gk->blacklist) {
        fprintf(stderr, "[Gatekeeper] Failed to create blacklist BitVector\n");
        return -1;
    }

    /* Layer 2: dynamic watchlist (RS-vEB Tree) */
    gk->watchlist = veb_create();
    if (!gk->watchlist) {
        fprintf(stderr, "[Gatekeeper] Failed to create watchlist VebTree\n");
        bv_destroy(gk->blacklist);
        return -1;
    }

    gk->blacklist_count = 0;
    gk->drop_count      = 0;
    gk->pass_count      = 0;

    printf("[Gatekeeper] Initialised. Layer 1 (BitVector) + Layer 2 (RS-vEB) ready.\n");
    return 0;
}

void gk_destroy(Gatekeeper *gk) {
    if (!gk) return;
    bv_destroy(gk->blacklist);
    veb_destroy(gk->watchlist);
    gk->blacklist = NULL;
    gk->watchlist = NULL;
}

/*
 * gk_load_blacklist — Parse a text file of IPs and load into Layer 1.
 *
 * Supports two formats per line:
 *   Dotted-decimal:  "192.168.1.1"
 *   Raw hex uint32:  "0xC0A80101"
 *
 * Lines starting with '#' are treated as comments and skipped.
 * The BitVector stores the TOP 16 bits of each IP for an O(1) prefix check.
 * Full 32-bit precision is in the watchlist if needed.
 *
 * NOTE: For a production blacklist (e.g. Spamhaus DROP list), you would
 * typically store /24 or /16 CIDR ranges rather than individual IPs.
 * This loader handles individual IPs as a starting point.
 */
int gk_load_blacklist(Gatekeeper *gk, const char *filepath) {
    if (!gk || !filepath) return -1;

    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "[Gatekeeper] Cannot open blacklist file: %s\n", filepath);
        return -1;
    }

    char   line[64];
    int    loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0') continue;

        uint32_t ip = 0;

        if (strncmp(line, "0x", 2) == 0 || strncmp(line, "0X", 2) == 0) {
            /* Hex format */
            ip = (uint32_t)strtoul(line, NULL, 16);
        } else {
            /* Dotted-decimal format — manual parse (Windows compatible) */
            unsigned int a, b, c, d;
            if (sscanf(line, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
                a > 255 || b > 255 || c > 255 || d > 255) {
                fprintf(stderr, "[Gatekeeper] Skipping invalid IP: %s\n", line);
                continue;
            }
            ip = (a << 24) | (b << 16) | (c << 8) | d;
        }
        
        /*
         * Store the top 16 bits in the BitVector for the fast first-pass.
         * This flags entire /16 subnets — fast but slightly over-broad.
         * Layer 2 (vEB) is then used for exact 32-bit confirmation.
         */
        uint16_t top = (uint16_t)(ip >> 16);
        bv_set(gk->blacklist, top);
        veb_insert(gk->watchlist, ip);   /* Exact entry in Layer 2 */

        loaded++;
        gk->blacklist_count++;

        if (loaded >= GK_MAX_BLACKLIST_IPS) {
            fprintf(stderr, "[Gatekeeper] Blacklist cap (%d) reached.\n",
                    GK_MAX_BLACKLIST_IPS);
            break;
        }
    }

    fclose(f);
    printf("[Gatekeeper] Loaded %d IPs from %s\n", loaded, filepath);
    return loaded;
}

void gk_watchlist_add(Gatekeeper *gk, uint32_t ip) {
    if (!gk) return;
    veb_insert(gk->watchlist, ip);
}

void gk_watchlist_remove(Gatekeeper *gk, uint32_t ip) {
    if (!gk) return;
    veb_delete(gk->watchlist, ip);
}

/*
 * check_ip — The core per-packet decision function.
 *
 * Design:
 *   Layer 1 uses the BitVector on the top-16-bit prefix. This is an
 *   extremely cheap check: a hash + bit read. If the /16 prefix is NOT
 *   in the blacklist at all, we skip Layer 2 entirely and pass.
 *
 *   If the prefix IS flagged, we do the exact 32-bit lookup in the vEB
 *   tree to avoid false positives from prefix-level tagging.
 *
 *   The watchlist (dynamic additions from ML engine) is checked after.
 */
int check_ip(Gatekeeper *gk, uint32_t ip) {
    if (!gk) return 0;  /* Fail-open if misconfigured */

    uint16_t top = (uint16_t)(ip >> 16);

    /* --- LAYER 1: BitVector prefix check O(1) --- */
    if (bv_contains(gk->blacklist, top)) {
        /* The /16 prefix is flagged — do exact lookup in vEB */
        if (veb_member(gk->watchlist, ip)) {
            gk->drop_count++;
            return 1;  /* DROP: confirmed blacklisted IP */
        }
        /* Prefix match but not exact — could be a /16 false positive */
        /* Fall through to pass (or add stricter CIDR logic here) */
    }

    /* --- LAYER 2: Dynamic watchlist check O(1) amortised --- */
    /*
     * This catches IPs flagged by the Python ML engine that were added
     * AFTER the initial blacklist load (real-time threat response).
     * Note: blacklisted IPs are also in the watchlist (inserted during load),
     * but we check the watchlist separately here for ML-only additions.
     */
    if (veb_member(gk->watchlist, ip)) {
        gk->drop_count++;
        return 1;  /* DROP: on dynamic watchlist */
    }

    gk->pass_count++;
    return 0;  /* PASS */
}

void gk_print_stats(const Gatekeeper *gk) {
    if (!gk) return;
    printf("[Gatekeeper Stats]\n");
    printf("  Blacklist IPs loaded : %u\n",   gk->blacklist_count);
    printf("  Watchlist IPs active : %u\n",   gk->watchlist ? gk->watchlist->count : 0);
    printf("  Packets dropped      : %u\n",   gk->drop_count);
    printf("  Packets passed       : %u\n",   gk->pass_count);
    uint32_t total = gk->drop_count + gk->pass_count;
    if (total > 0) {
        printf("  Drop rate            : %.2f%%\n",
               100.0f * gk->drop_count / total);
    }
}
