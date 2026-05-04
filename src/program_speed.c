#include <stdio.h>
#include <time.h>
#include "program_speed.h"
#include "helpers.h"

void ps_cap(
    struct timespec *point_in_time
) {
    clock_gettime(CLOCK_MONOTONIC, point_in_time);
}

void ps_print_pit(
    struct timespec *point_in_time,
    int *tid
) {
    printfid("Time: %ld ns (%.3f ms)", *tid, point_in_time->tv_sec, point_in_time->tv_nsec / 1e6);
}

void ps_print_elapsed(
    struct program_speed *ps,
    int *tid
) {
    long elapsed_ns;

    elapsed_ns = (ps->end.tv_sec - ps->start.tv_sec) * 1000000000L
                    + (ps->end.tv_nsec - ps->start.tv_nsec);
    printfid("Elapsed: %ld ns (%.3f ms)", *tid, elapsed_ns, elapsed_ns / 1e6);
}