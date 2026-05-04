#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "list_node.h"

void enqueue(ListNodeManager *q, int client_fd) {
    ListNode *node = malloc(sizeof(ListNode));
    node->fd = client_fd;
    node->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail) {
		q->tail->next = node;
	} else {
		q->head = node;
	}
	
	q->tail = node;
    pthread_cond_signal(&q->ready);
    pthread_mutex_unlock(&q->lock);
}

int dequeue(ListNodeManager *q) {
    int fd;
    ListNode *node;

    pthread_mutex_lock(&q->lock);
    
    while (q->head == NULL) {
        pthread_cond_wait(&q->ready, &q->lock);
    }

    node = q->head;
    fd = node->fd;
    q->head = node->next;

    if (q->head == NULL) {
        q->tail = NULL;
        // queue is now empty
    }
    
    pthread_mutex_unlock(&q->lock);
    free(node);

    return fd;
}