#ifndef CAPTURE_H
#define CAPTURE_H

/*
 * capture.h — ArborFlow Capture Engine
 * --------------------------------------
 * Member 1: Network Packet Ingestion
 *
 * Uses libpcap to sniff packets from network interface.
 * Parses Ethernet/IP/TCP/UDP headers to extract packet fields.
 * Pushes packets into concurrent queue for processing threads.
 */

#include <pcap.h>
#include <pthread.h>
#include "../scheduler/packet.h"
#include "../include/concurrent_q.h"

#define CAPTURE_SNAPLEN  65535  /* Maximum bytes to capture per packet */
#define CAPTURE_PROMISC  1      /* Promiscuous mode */
#define CAPTURE_TIMEOUT  1000   /* Read timeout in ms */

/* Capture engine state */
typedef struct {
    pcap_t          *handle;        /* libpcap handle */
    char            *device;        /* Network interface name */
    ConcurrentQueue *queue;         /* Queue to push captured packets */
    pthread_t       capture_thread; /* Thread for packet capture */
    int             running;        /* Flag to stop capture */
} CaptureEngine;

/*
 * capture_init — Initialize capture engine with network device.
 *                  Returns 0 on success, -1 on failure.
 */
int capture_init(CaptureEngine *ce, const char *device, ConcurrentQueue *queue);

/*
 * capture_start — Start packet capture in background thread.
 *                 Returns 0 on success, -1 on failure.
 */
int capture_start(CaptureEngine *ce);

/*
 * capture_stop — Stop capture and cleanup.
 */
void capture_stop(CaptureEngine *ce);

/*
 * capture_list_devices — List available network devices.
 */
void capture_list_devices(void);

#endif