#include <stdio.h>
#include <stdlib.h>
#include "http_response.h"
#include "line_in_memory_array.h"
#include "xmalloc.h"
#include "array_append.h"

void freeHTTPResponse(
	HTTPResponse *htr
) {
	freelima(htr->headers);
}

int add_header(
	HTTPResponse *htr,
	char *header 
) {
	// update the size of the body
	// TODO: #define at TOF
	size_t header_size = 256;
	char *new_header = xmalloc(header_size);
	if (new_header == NULL) return -1;
	LineInMemory new_lim = lim(new_header, header_size);
	arr_append((*htr->headers), new_lim);
	return 0;
}