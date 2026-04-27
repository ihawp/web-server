#pragma once

#include <time.h>

typedef struct {
    struct timespec start, end;
} ProgramSpeed;

void start(
    ProgramSpeed *ps
);

void end(
    ProgramSpeed *ps
);