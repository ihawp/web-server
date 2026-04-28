#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    int epc, sfd;
    pthread_mutex_t lock;
    pthread_cond_t ready;
} WorkerData;