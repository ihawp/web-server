#pragma once

#include <unistd.h>
#include <string.h>
#include "line_in_memory_array.h"
#include "string_view.h"

// TODO: rename and organize and refactor and rethink!
#define MAX_EVENTS 10
#define MAX_WORKERS 3
#define HEADER_SIZE 256
#define REQ_METHOD_SIZE 16
#define REQ_PATH_SIZE 256
#define REQ_HTTP_VERSION_SIZE 24
#define MAX_CONTENT_LENGTH 8000
#define CLIENT_BUF_SIZE 1024
#define CHUNK_SIZE 512
#define RESPONSE_BUF_SIZE 512
#define PATH_SIZE 512
#define CHAR_SIZE sizeof(char)

typedef struct {
	LIMArray *headers;
	char *body;
	long content_length;
	char method[REQ_METHOD_SIZE];
	char path[REQ_PATH_SIZE];
	char http_version[REQ_HTTP_VERSION_SIZE];
} HTTPRequest;

typedef struct {
	LIMArray *headers;
	StringView body;
	int status;
} HTTPResponse;

void freeHTTPRequest(
	HTTPRequest *hrq
);

void freeHTTPResponse(
	HTTPResponse *htr
);

int add_header(
	HTTPResponse *htr,
	char *header 
);

char *file_to_content_type(
	char *path
);

const char *http_status_str(
	int code
);

void send_json_response(
	int *client_fd,
	int status,
	char *error_message	
);

// This should accept an already open and tested fd, rather than have the 404 loop if f == null and what not.
int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
);

int parse_headers(
	HTTPRequest *http_request,
	HTTPResponse *http_response
);

int extract_path_method_version(
	HTTPRequest *req
);

int capture_headers(
	int *client_fd,
	HTTPRequest *req
);

char *recv_header_chunks(
	int *client_fd,
	char *buffer,
	ssize_t *recv_count
);

// TODO: post requests get stuck
int recv_body_chunks(
	int *client_fd,
	char *buffer,
	size_t content_length,
	ssize_t *total,
	size_t *body_length
);

LIMArray find_header_bounds(
	char *req
);

int handle_request(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
);

void *http_worker(
	void *data
);

void http_server(
	int sfd,
	char *port
);