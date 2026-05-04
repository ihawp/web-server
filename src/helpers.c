#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <fcntl.h>
#include "helpers.h"

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

void printfid(
	const char *format, 
	pid_t id, 
	...
) {
	va_list args;
    va_start(args, id);
    printf("[%d]: ", id);
    vprintf(format, args);
	printf("\n");
    va_end(args);
}