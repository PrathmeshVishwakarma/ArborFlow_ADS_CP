#ifndef GATEKEEPER_H
#define GATEKEEPER_H

/*
 * gatekeeper.h — ArborFlow Filtering Gatekeeper
 * -----------------------------------------------
 * Member 2 public interface.
 *
 * Provides a two-layer IP filtering pipeline:
 *
 *   Layer 1 — BitVector  (O(1))          — static global blacklist
 *   Layer 2 — RS-vEB Tree (O(1) amortized) — dynamic suspicious watchlist
 *
 * Usage:
 *   1. Call gk_init() once at startup.
 *   2. Call gk_load_blacklist() to populate Layer 1 from a file.
 *   3. Call check_ip(ip) per packet from the sniffer.
 *   4. Call gk_destroy() on shutdown.
 */

#include <stdint.h>
#include "../include/bit_vector.h"
#include "../include/veb_tree.h"

/* Maximum number of IPs loadable from a blacklist file */
#define GK_MAX_BLACKLIST_IPS  (1 << 20)  /* 1,048,576 entries */

/* ------------------------------------------------------------------ */
/*  Gatekeeper state (one global instance per engine)                  */
/* ------------------------------------------------------------------ */

typedef struct {
    BitVector *blacklist;       /* Layer 1: known-bad IPs (static)    */
    VebTree   *watchlist;       /* Layer 2: suspicious IPs (dynamic)  */
    uint32_t   blacklist_count; /* Number of IPs loaded into Layer 1  */
    uint32_t   drop_count;      /* Running total of dropped packets   */
    uint32_t   pass_count;      /* Running total of passed packets    */
} Gatekeeper;

/*
 * gk_init     — Initialise the gatekeeper. Must be called before anything else.
 *               Returns 0 on success, -1 on failure.
 */
int gk_init(Gatekeeper *gk);

/*
 * gk_destroy  — Free all memory. Call on shutdown.
 */
void gk_destroy(Gatekeeper *gk);

/*
 * gk_load_blacklist — Read newline-separated IPv4 addresses (dotted-decimal
 *                     or raw uint32 hex) from `filepath` and insert into
 *                     Layer 1. Returns number of IPs loaded, -1 on error.
 */
int gk_load_blacklist(Gatekeeper *gk, const char *filepath);

/*
 * gk_watchlist_add    — Dynamically add a suspicious IP to Layer 2.
 *                        Called by the Python ML bridge when anomalies detected.
 */
void gk_watchlist_add(Gatekeeper *gk, uint32_t ip);

/*
 * gk_watchlist_remove — Remove an IP from the watchlist (e.g. after timeout).
 */
void gk_watchlist_remove(Gatekeeper *gk, uint32_t ip);

/*
 * check_ip — Core gatekeeper function. Called per packet by the sniffer.
 *
 *   Returns: 1 → DROP the packet (malicious)
 *            0 → PASS the packet (clean)
 *
 *   Pipeline:
 *     1. Layer 1 (BitVector):  if blacklisted → DROP immediately
 *     2. Layer 2 (vEB Tree):   if on watchlist → DROP
 *     3. Otherwise             → PASS
 */
int check_ip(Gatekeeper *gk, uint32_t ip);

/*
 * gk_print_stats — Print drop/pass counters to stdout.
 */
void gk_print_stats(const Gatekeeper *gk);

#endif /* GATEKEEPER_H */
