#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct ListNode {
    int fd;
    struct ListNode *next;
} ListNode;

typedef struct {
    int epc;
    ListNode *head;
    ListNode *tail;
    pthread_mutex_t lock;
    pthread_cond_t ready;
} ListNodeManager;

void enqueue(
    ListNodeManager *q, 
    int client_fd
);

int dequeue(ListNodeManager *q);