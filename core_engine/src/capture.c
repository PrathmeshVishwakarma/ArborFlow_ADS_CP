#include "../include/capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>

/*
 * capture.c — libpcap Packet Capture Implementation
 * --------------------------------------------------
 * Sniffs packets from network interface, parses headers,
 * extracts Packet fields, and enqueues for processing.
 */

/* Packet callback function for libpcap */
void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    CaptureEngine *ce = (CaptureEngine *)user;

    /* Parse Ethernet header */
    struct ether_header *eth = (struct ether_header *)packet;
    if (ntohs(eth->ether_type) != ETHERTYPE_IP) {
        return;  /* Skip non-IP packets */
    }

    /* Parse IP header */
    struct ip *ip_hdr = (struct ip *)(packet + sizeof(struct ether_header));
    uint32_t src_ip = ntohl(ip_hdr->ip_src.s_addr);
    uint32_t dst_ip = ntohl(ip_hdr->ip_dst.s_addr);
    int protocol = ip_hdr->ip_p;
    int packet_size = header->len;  /* Total packet length */

    /* Determine priority (simple heuristic) */
    int priority = 5;  /* Default */
    if (protocol == IPPROTO_TCP) {
        struct tcphdr *tcp_hdr = (struct tcphdr *)((u_char *)ip_hdr + (ip_hdr->ip_hl * 4));
        if (ntohs(tcp_hdr->th_dport) == 80 || ntohs(tcp_hdr->th_dport) == 443) {
            priority = 8;  /* HTTP/HTTPS higher priority */
        }
    } else if (protocol == IPPROTO_UDP) {
        struct udphdr *udp_hdr = (struct udphdr *)((u_char *)ip_hdr + (ip_hdr->ip_hl * 4));
        if (ntohs(udp_hdr->uh_dport) == 53) {
            priority = 9;  /* DNS high priority */
        }
    }

    /* Create Packet struct */
    Packet pkt = {
        .src_ip = src_ip,
        .dest_ip = dst_ip,
        .protocol = protocol,
        .size = packet_size,
        .priority = priority
    };

    /* Enqueue packet (non-blocking) */
    if (!cq_enqueue(ce->queue, pkt)) {
        /* Queue full - drop packet (or handle differently) */
        fprintf(stderr, "[Capture] Queue full, dropping packet\n");
    }
}

/* Capture thread function */
void *capture_thread_func(void *arg) {
    CaptureEngine *ce = (CaptureEngine *)arg;

    /* Start packet capture loop */
    pcap_loop(ce->handle, -1, packet_handler, (u_char *)ce);

    return NULL;
}

int capture_init(CaptureEngine *ce, const char *device, ConcurrentQueue *queue) {
    if (!ce || !device || !queue) return -1;

    memset(ce, 0, sizeof(CaptureEngine));
    ce->device = strdup(device);
    ce->queue = queue;
    ce->running = 0;

    /* Open device for live capture */
    char errbuf[PCAP_ERRBUF_SIZE];
    ce->handle = pcap_open_live(device, CAPTURE_SNAPLEN, CAPTURE_PROMISC,
                                CAPTURE_TIMEOUT, errbuf);
    if (!ce->handle) {
        fprintf(stderr, "[Capture] pcap_open_live failed: %s\n", errbuf);
        free(ce->device);
        return -1;
    }

    /* Compile and set filter (optional: capture only IP packets) */
    struct bpf_program fp;
    char filter_exp[] = "ip";  /* Capture only IP packets */
    if (pcap_compile(ce->handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "[Capture] pcap_compile failed: %s\n", pcap_geterr(ce->handle));
        pcap_close(ce->handle);
        free(ce->device);
        return -1;
    }
    if (pcap_setfilter(ce->handle, &fp) == -1) {
        fprintf(stderr, "[Capture] pcap_setfilter failed: %s\n", pcap_geterr(ce->handle));
        pcap_freecode(&fp);
        pcap_close(ce->handle);
        free(ce->device);
        return -1;
    }
    pcap_freecode(&fp);

    return 0;
}

int capture_start(CaptureEngine *ce) {
    if (!ce || ce->running) return -1;

    ce->running = 1;

    /* Create capture thread */
    if (pthread_create(&ce->capture_thread, NULL, capture_thread_func, ce) != 0) {
        fprintf(stderr, "[Capture] Failed to create capture thread\n");
        ce->running = 0;
        return -1;
    }

    printf("[Capture] Started on device %s\n", ce->device);
    return 0;
}

void capture_stop(CaptureEngine *ce) {
    if (!ce) return;

    ce->running = 0;
    pcap_breakloop(ce->handle);  /* Stop pcap_loop */

    /* Wait for thread to finish */
    pthread_join(ce->capture_thread, NULL);

    /* Cleanup */
    pcap_close(ce->handle);
    free(ce->device);

    printf("[Capture] Stopped\n");
}

void capture_list_devices(void) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs;

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "[Capture] pcap_findalldevs failed: %s\n", errbuf);
        return;
    }

    printf("Available network devices:\n");
    for (pcap_if_t *d = alldevs; d; d = d->next) {
        printf("  %s", d->name);
        if (d->description) {
            printf(" (%s)", d->description);
        }
        printf("\n");
    }

    pcap_freealldevs(alldevs);
}