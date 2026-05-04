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
	size_t capacity,
	char *storage_location
) {
	return (LIMArray) {
		.pointer = pointer,
		.count = count,
		.capacity = capacity,
		.storage_location = storage_location
	};
}

void freelim(
	LineInMemory *line
) {
	line->pointer = NULL;
	line->count = 0;
}

void freelima(
	LIMArray *arr
) {
	int i;

	if (!arr) return;

	for (i = 0; i < arr->count; i++) {
		freelim(&arr->pointer[i]);
	}
	
	memset(arr->storage_location, 0, strlen(arr->storage_location));
	free(arr->storage_location);
	free(arr->pointer);
	
	arr->storage_location = NULL;
	arr->pointer = NULL;
	arr->count = 0;
	arr->capacity = 0;
}