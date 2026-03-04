#include <stdio.h>
#include "../include/gatekeeper.h"

int main(void) {
    Gatekeeper gk;
    gk_init(&gk);

    // Add any IPs you want to block
    gk_watchlist_add(&gk, 0xC0A80101);  // 192.168.1.1
    gk_watchlist_add(&gk, 0x08080808);  // 8.8.8.8

    // Test any IPs you want to check
    uint32_t test_ips[] = {
        0xC0A80101,  // 192.168.1.1  — should DROP
        0x08080808,  // 8.8.8.8      — should DROP
        0x0A000001,  // 10.0.0.1     — should PASS
        0x01010101,  // 1.1.1.1      — should PASS
    };

    for (int i = 0; i < 4; i++) {
        uint32_t ip = test_ips[i];
        printf("%u.%u.%u.%u → %s\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >>  8) & 0xFF,
            (ip      ) & 0xFF,
            check_ip(&gk, ip) ? "DROP" : "PASS"
        );
    }

    gk_print_stats(&gk);
    gk_destroy(&gk);
    return 0;
}