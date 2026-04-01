#include "../include/concurrent_q.h"
#include <string.h>

/*
 * concurrent_q.c — Lock-Free SPSC Queue Implementation
 * -----------------------------------------------------
 * Uses atomic operations for thread safety without locks.
 * Assumes single producer and single consumer.
 */

ConcurrentQueue *cq_create(void) {
    ConcurrentQueue *q = (ConcurrentQueue *)malloc(sizeof(ConcurrentQueue));
    if (!q) return NULL;

    memset(q, 0, sizeof(ConcurrentQueue));
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);

    return q;
}

void cq_destroy(ConcurrentQueue *q) {
    if (q) free(q);
}

int cq_enqueue(ConcurrentQueue *q, Packet p) {
    unsigned int tail = atomic_load(&q->tail);
    unsigned int next_tail = (tail + 1) % Q_SIZE;

    /* Check if queue is full */
    if (next_tail == atomic_load(&q->head)) {
        return 0;  /* Full */
    }

    /* Store packet */
    q->buffer[tail] = p;

    /* Update tail atomically */
    atomic_store(&q->tail, next_tail);
    return 1;
}

int cq_dequeue(ConcurrentQueue *q, Packet *p) {
    unsigned int head = atomic_load(&q->head);

    /* Check if queue is empty */
    if (head == atomic_load(&q->tail)) {
        return 0;  /* Empty */
    }

    /* Retrieve packet */
    *p = q->buffer[head];

    /* Update head atomically */
    atomic_store(&q->head, (head + 1) % Q_SIZE);
    return 1;
}

int cq_empty(ConcurrentQueue *q) {
    return atomic_load(&q->head) == atomic_load(&q->tail);
}

int cq_full(ConcurrentQueue *q) {
    unsigned int tail = atomic_load(&q->tail);
    unsigned int next_tail = (tail + 1) % Q_SIZE;
    return next_tail == atomic_load(&q->head);
}