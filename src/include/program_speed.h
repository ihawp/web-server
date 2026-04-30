#pragma once

#include <time.h>

struct program_speed {
    struct timespec start, end;
};

void ps_cap(
    struct timespec *point_in_time
);

// prints a specific point in time
void ps_print_pit(
    struct timespec *point_in_time,
    int *tid
);

// prints the elapsed time
void ps_print_elapsed(
    struct program_speed *ps,
    int *tid
);