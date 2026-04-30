#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct process_data {
    int epc, sfd;
    pid_t pid;
    pthread_mutex_t lock;
    pthread_cond_t ready;
};