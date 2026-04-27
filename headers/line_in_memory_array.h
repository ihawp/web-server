#pragma once

#include "string_view.h"

typedef struct {
	char *pointer;
	size_t count;
} LineInMemory;

typedef struct {
	LineInMemory *pointer;
	size_t count;
	size_t capacity;
} LIMArray;

LineInMemory lim(
	char *pointer,
	size_t count
);

LIMArray lima(
	LineInMemory *pointer,
	size_t count,
	size_t capacity
);

void freelima(
	LIMArray *arr
);

LIMArray find_header_bounds(
	char *req
);