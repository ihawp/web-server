#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define arr_append(arr, item) { \
	if ((arr).count + 1 >= (arr).capacity) { \
		if ((arr).capacity == 0) (arr).capacity = 256; \
		size_t new_alloc_amount = (arr).capacity * 2 * sizeof(*(arr).pointer); \
		void *new = realloc((arr).pointer, new_alloc_amount); \
		if (new == NULL) { \
			printf("Failed to reallocate\n"); \
			(arr).pointer = NULL; \
		} else { \
			(arr).pointer = new; \
			(arr).capacity *= 2; \
		} \
	} \
	(arr).count += 1; \
	(arr).pointer[(arr).count - 1] = item; \
} \

void *xmalloc(
	size_t size
);

void printfid(
	const char *format, 
	int id, 
	...
);