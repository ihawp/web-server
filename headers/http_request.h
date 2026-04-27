#pragma once

#include <string.h>
#include "line_in_memory_array.h"

#define REQ_METHOD_SIZE 16
#define REQ_PATH_SIZE 256
#define REQ_HTTP_VERSION_SIZE 24

typedef struct {
	LIMArray *headers;
	char *body;
	long double content_length;
	char method[REQ_METHOD_SIZE];
	char path[REQ_PATH_SIZE];
	char http_version[REQ_HTTP_VERSION_SIZE];
} HTTPRequest;

void freeHTTPRequest(
	HTTPRequest *hrq
);