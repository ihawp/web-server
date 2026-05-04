#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "line_in_memory_array.h"
#include "helpers.h"

LineInMemory lim(
	char *pointer,
	size_t count
) {
	return (LineInMemory) {
		.pointer = pointer,
		.count = count
	};
}

LIMArray lima(
	LineInMemory *pointer,
	size_t count,
	size_t capacity
) {
	return (LIMArray) {
		.pointer = pointer,
		.count = count,
		.capacity = capacity
	};
}

void freelim(
	LineInMemory *line
) {
	// free the memory allocated for this pointer
	line->pointer = NULL;
	line->count = 0;
}

void freelima(
	LIMArray *arr
) {
	if (!arr) return;

	// need to loop over the items and free them individually
	for (int i = 0; i < arr->count; i++) {
		freelim(&arr->pointer[i]);
	}

	free(arr->pointer);
	arr->pointer = NULL;
	arr->count = 0;
	arr->capacity = 0;
}