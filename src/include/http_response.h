#pragma once

#include <string.h>
#include "line_in_memory_array.h"
#include "string_view.h"

typedef struct {
	LIMArray *headers;
	StringView body; 
} HTTPResponse;

void freeHTTPResponse(
	HTTPResponse *htr
);

int add_header(
	HTTPResponse *htr,
	char *header 
);