/*
 * test_gatekeeper.c — ArborFlow Gatekeeper Test Suite
 * -----------------------------------------------------
 * Run with: make test
 *
 * Tests:
 *   1. BitVector — set, contains, clear, reset
 *   2. VebTree   — insert, member, delete, empty
 *   3. Gatekeeper — init, check_ip (pass/drop), watchlist add/remove
 *   4. Edge cases — duplicate inserts, delete non-existent, boundary IPs
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "../include/bit_vector.h"
#include "../include/veb_tree.h"
#include "../include/gatekeeper.h"

/* Simple test harness */
static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name)  do { printf("  %-45s ", name); tests_run++; } while(0)
#define PASS()      do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg)   do { printf("FAIL — %s\n", msg); } while(0)

/* ------------------------------------------------------------------ */
/*  BitVector Tests                                                    */
/* ------------------------------------------------------------------ */

static void test_bitvector(void) {
    printf("\n[1] BitVector\n");

    TEST("bv_create returns non-NULL");
    BitVector *bv = bv_create(1024);
    if (bv) PASS(); else FAIL("returned NULL");

    TEST("bv_contains returns 0 on fresh vector");
    if (!bv_contains(bv, 0) && !bv_contains(bv, 500) && !bv_contains(bv, 1023))
        PASS(); else FAIL("bits not zero after creation");

    TEST("bv_set and bv_contains");
    bv_set(bv, 42);
    bv_set(bv, 999);
    if (bv_contains(bv, 42) && bv_contains(bv, 999))
        PASS(); else FAIL("set bits not readable");

    TEST("bv_clear removes bit");
    bv_clear(bv, 42);
    if (!bv_contains(bv, 42) && bv_contains(bv, 999))
        PASS(); else FAIL("clear did not work");

    TEST("bv_reset clears all bits");
    bv_reset(bv);
    if (!bv_contains(bv, 999))
        PASS(); else FAIL("reset did not clear all bits");

    TEST("bv_contains boundary — index 0 and capacity-1");
    bv_set(bv, 0);
    bv_set(bv, 1023);
    if (bv_contains(bv, 0) && bv_contains(bv, 1023))
        PASS(); else FAIL("boundary bits failed");

    TEST("bv_contains out-of-bounds returns 0 (no crash)");
    int r = bv_contains(bv, 99999);  /* Way out of bounds */
    if (r == 0) PASS(); else FAIL("out-of-bounds did not return 0");

    bv_destroy(bv);

    TEST("bv for 65536 entries (cluster size)");
    BitVector *bv2 = bv_create(1 << 16);
    bv_set(bv2, 0);
    bv_set(bv2, 65535);
    if (bv_contains(bv2, 0) && bv_contains(bv2, 65535) && !bv_contains(bv2, 32768))
        PASS(); else FAIL("65536-entry vector failed");
    bv_destroy(bv2);
}

/* ------------------------------------------------------------------ */
/*  VebTree Tests                                                      */
/* ------------------------------------------------------------------ */

static void test_vebtree(void) {
    printf("\n[2] RS-vEB Tree\n");

    TEST("veb_create returns non-NULL");
    VebTree *veb = veb_create();
    if (veb) PASS(); else FAIL("returned NULL");

    TEST("veb_is_empty on fresh tree");
    if (veb_is_empty(veb)) PASS(); else FAIL("new tree not empty");

    TEST("veb_member returns 0 before any insert");
    if (!veb_member(veb, 0xC0A80101)) PASS(); else FAIL("member on empty tree");

    TEST("veb_insert and veb_member (192.168.1.1)");
    veb_insert(veb, 0xC0A80101);
    if (veb_member(veb, 0xC0A80101)) PASS(); else FAIL("member after insert");

    TEST("veb_member false for non-inserted IP");
    if (!veb_member(veb, 0xC0A80102)) PASS(); else FAIL("false positive");

    TEST("veb_insert multiple IPs across different /16 prefixes");
    veb_insert(veb, 0x08080808);  /* 8.8.8.8    Google DNS */
    veb_insert(veb, 0x01010101);  /* 1.1.1.1    Cloudflare */
    veb_insert(veb, 0xDEADBEEF);  /* 222.173.190.239 */
    if (veb_member(veb, 0x08080808) &&
        veb_member(veb, 0x01010101) &&
        veb_member(veb, 0xDEADBEEF))
        PASS(); else FAIL("multi-prefix inserts failed");

    TEST("veb_delete removes an IP");
    veb_delete(veb, 0x08080808);
    if (!veb_member(veb, 0x08080808)) PASS(); else FAIL("delete did not remove");

    TEST("veb_delete does not affect other IPs");
    if (veb_member(veb, 0xC0A80101) && veb_member(veb, 0x01010101))
        PASS(); else FAIL("delete removed wrong IPs");

    TEST("veb_insert duplicate does not corrupt count");
    uint32_t before = veb->count;
    veb_insert(veb, 0xC0A80101);  /* Already present */
    if (veb->count == before) PASS(); else FAIL("duplicate changed count");

    TEST("veb boundary IPs — 0x00000000 and 0xFFFFFFFF");
    veb_insert(veb, 0x00000000);
    veb_insert(veb, 0xFFFFFFFF);
    if (veb_member(veb, 0x00000000) && veb_member(veb, 0xFFFFFFFF))
        PASS(); else FAIL("boundary IPs failed");

    veb_destroy(veb);
}

/* ------------------------------------------------------------------ */
/*  Gatekeeper Integration Tests                                       */
/* ------------------------------------------------------------------ */

static void test_gatekeeper(void) {
    printf("\n[3] Gatekeeper Integration\n");

    TEST("gk_init returns 0");
    Gatekeeper gk;
    if (gk_init(&gk) == 0) PASS(); else FAIL("init failed");

    TEST("check_ip PASS on clean IP (10.0.0.1)");
    if (check_ip(&gk, 0x0A000001) == 0) PASS(); else FAIL("clean IP dropped");

    TEST("gk_watchlist_add then check_ip DROPS the IP");
    gk_watchlist_add(&gk, 0xBAD00001);
    if (check_ip(&gk, 0xBAD00001) == 1) PASS(); else FAIL("watchlist IP not dropped");

    TEST("check_ip PASS for IP not on watchlist");
    if (check_ip(&gk, 0xBAD00002) == 0) PASS(); else FAIL("clean IP incorrectly dropped");

    TEST("gk_watchlist_remove then check_ip PASSES");
    gk_watchlist_remove(&gk, 0xBAD00001);
    if (check_ip(&gk, 0xBAD00001) == 0) PASS(); else FAIL("IP still dropped after remove");

    TEST("Multiple watchlist entries — all dropped");
    gk_watchlist_add(&gk, 0xDEAD0001);
    gk_watchlist_add(&gk, 0xDEAD0002);
    gk_watchlist_add(&gk, 0xDEAD0003);
    if (check_ip(&gk, 0xDEAD0001) == 1 &&
        check_ip(&gk, 0xDEAD0002) == 1 &&
        check_ip(&gk, 0xDEAD0003) == 1)
        PASS(); else FAIL("not all watchlist IPs dropped");

    TEST("drop_count and pass_count updated correctly");
    /* At this point: 3 drops (DEAD0001-3), multiple passes */
    if (gk.drop_count >= 3 && gk.pass_count >= 2)
        PASS(); else FAIL("counters not updated");

    TEST("gk_print_stats runs without crash");
    gk_print_stats(&gk);
    PASS();

    gk_destroy(&gk);

    TEST("check_ip after destroy returns 0 (fail-open)");
    Gatekeeper gk2;
    gk_init(&gk2);
    gk_destroy(&gk2);
    gk2.blacklist = NULL;
    gk2.watchlist = NULL;
    if (check_ip(&gk2, 0x12345678) == 0) PASS(); else FAIL("fail-open violated");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("==============================================\n");
    printf("  ArborFlow Gatekeeper — Test Suite\n");
    printf("  Member 2: Filtering Gatekeeper Module\n");
    printf("==============================================\n");

    test_bitvector();
    test_vebtree();
    test_gatekeeper();

    printf("\n==============================================\n");
    printf("  Results: %d / %d tests passed\n", tests_passed, tests_run);
    printf("==============================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
