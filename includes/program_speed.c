#include "program_speed.h"
#include <stdio.h>
#include <time.h>

void start(
    ProgramSpeed *ps
) {
    clock_gettime(CLOCK_MONOTONIC, &ps->start);
}

void end(
    ProgramSpeed *ps
) {
    clock_gettime(CLOCK_MONOTONIC, &ps->end);
    long elapsed_ns = (ps->end.tv_sec - ps->start.tv_sec) * 1000000000L
                    + (ps->end.tv_nsec - ps->start.tv_nsec);
    printf("Elapsed: %ld ns (%.3f ms)\n", elapsed_ns, elapsed_ns / 1e6);
}