#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/gatekeeper.h"

/* Generate a random 32-bit IP */
static uint32_t random_ip(void) {
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

/* Print an IP from uint32 */
static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (ip >> 24) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >>  8) & 0xFF,
        (ip      ) & 0xFF);
}

int main(void) {
    srand((unsigned int)time(NULL));

    Gatekeeper gk;
    gk_init(&gk);

    /* -------------------------------------------------- */
    /* STEP 1: Load 100,000 "malicious" IPs into watchlist */
    /* -------------------------------------------------- */
    printf("\n[1] Loading 100,000 malicious IPs into watchlist...\n");

    uint32_t *blacklisted = (uint32_t *)malloc(100000 * sizeof(uint32_t));
    if (!blacklisted) {
        printf("malloc failed\n");
        return 1;
    }

    for (int i = 0; i < 100000; i++) {
        blacklisted[i] = random_ip();
        gk_watchlist_add(&gk, blacklisted[i]);
    }
    printf("    Done. Watchlist loaded.\n");

    /* -------------------------------------------------- */
    /* STEP 2: Fire 1,000,000 packets through check_ip    */
    /* -------------------------------------------------- */
    printf("\n[2] Firing 1,000,000 packets through check_ip...\n");

    clock_t start = clock();

    int dropped = 0;
    int passed  = 0;

    for (int i = 0; i < 1000000; i++) {
        uint32_t ip;

        /*
         * Every 10th packet is a known-malicious IP from our blacklist.
         * The rest are random — some may coincidentally match, most won't.
         */
        if (i % 10 == 0) {
            ip = blacklisted[i % 100000];  /* Guaranteed malicious */
        } else {
            ip = random_ip();              /* Random — mostly clean */
        }

        if (check_ip(&gk, ip)) {
            dropped++;
        } else {
            passed++;
        }
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    /* -------------------------------------------------- */
    /* STEP 3: Results                                     */
    /* -------------------------------------------------- */
    printf("\n[3] Results\n");
    printf("    Total packets processed : 1,000,000\n");
    printf("    Dropped (malicious)     : %d\n",   dropped);
    printf("    Passed  (clean)         : %d\n",   passed);
    printf("    Time elapsed            : %.4f seconds\n", elapsed);
    printf("    Throughput              : %.0f packets/sec\n", 1000000.0 / elapsed);

    printf("\n[4] Sample of blocked IPs (first 5 from watchlist):\n");
    for (int i = 0; i < 5; i++) {
        printf("    ");
        print_ip(blacklisted[i]);
        printf(" → %s\n", check_ip(&gk, blacklisted[i]) ? "DROP" : "PASS");
    }

    printf("\n[5] Full Gatekeeper Stats:\n");
    gk_print_stats(&gk);

    free(blacklisted);
    gk_destroy(&gk);
    return 0;
}