#include <stdio.h>
#include <stdlib.h>
#include "xmalloc.h"

void *xmalloc(
	size_t size
) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		printf("Failed to allocate memory.\n");
		return NULL;
	}
	return ptr;
}