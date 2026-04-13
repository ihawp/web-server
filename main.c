#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
// #include <poll.h>
// #include <pthread.h>

/*
TODO:
- Request header parsing (in progress)
- Custom response headers
- Cookies
- Unused Includes: poll.h, pthread.h
*/

// TODO: Organize and label better
#define CLIENT_BUF_SIZE 2048
#define CHUNK_SIZE 512
#define RESPONSE_BUF_SIZE 512
#define PATH_SIZE 256
#define CHAR_SIZE sizeof(char)
#define MAX_CONTENT_LENGTH 4096

// HTTPRequest
#define REQ_METHOD_SIZE 16
#define REQ_PATH_SIZE 256
#define REQ_HTTP_VERSION_SIZE 24

// Returning (arr) is lazy? Compiler expects type passed as arr as return.
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
) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		printf("Failed to allocate memory.\n");
		return NULL;
	}
	return ptr;
}

/* ##########################
       STRING OPERATIONS
   ######################### */
typedef struct {
	char *string;
	size_t count;
} StringView;

StringView sv(
	char *string
) {
	return (StringView) {
		.string = string,
		.count = strlen(string) 
	};
}

#define SV_fmt "%.*s"
#define SV_arg(s) (int) (s)->count, (s)->string
#define SV_print(s, string) printf("%s"SV_fmt"\n", string, SV_arg(s))
#define SV_to_memory(h, h_size, s) snprintf(h, h_size, SV_fmt, SV_arg(s))

void print_sv_string(
	StringView *sv_string
) {
	printf(SV_fmt, SV_arg(sv_string));
}

void remove_from_left(
	StringView *sv,
	size_t amount
) {
	if (sv->count == 0) return;
	if (amount > sv->count) amount = sv->count;
	sv->string += amount;
	sv->count -= amount;
}

void remove_from_right(
	StringView *sv,
	size_t amount
) {
	if (sv->count == 0) return;
	if (amount > sv->count) amount = sv->count;
	sv->count -= amount;
}

void delim_from_left(
	StringView *sv,
	char delim
) {
	while (sv->string[0] == delim) {
		sv->string += 1;
		sv->count -= 1;
	}	
}

void delim_from_right(
	StringView *sv,
	char delim
) {
	while (sv->string[sv->count - 1] == delim) {
		sv->count -= 1;
	}
}

void trim_by_delim(
	StringView *sv,
	char delim
) {
	delim_from_left(sv, delim);
	delim_from_right(sv, delim);
}

StringView split_by_delim(	
	StringView *stv,
	char delim
) {
	size_t i = 0;
	while (i < stv->count && stv->string[i] != delim) {
		i += 1;
	}

	if (i < stv->count) {
		StringView item = {
			.string = stv->string,
			.count = i
		};
		remove_from_left(stv, i + 1);
		return item;
	}

	StringView item = *stv;
	remove_from_left(stv, stv->count);
	return item;
}

/* ##########################
             LIMA
   ######################### */
typedef struct {
	char *pointer;
	size_t count;
} LineInMemory;

LineInMemory lim(
	char *pointer,
	size_t count
) {
	return (LineInMemory) {
		.pointer = pointer,
		.count = count
	};
}

typedef struct {
	LineInMemory *pointer;
	size_t count;
	size_t capacity;
} LIMArray;

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

void freelima(
	LIMArray *arr
) {
	if (!arr) return;
	free(arr->pointer);
	arr->pointer = NULL;
	arr->count = 0;
	arr->capacity = 0;
}

/* ##########################
            REQUEST
   ######################### */
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
) {
	freelima(hrq->headers);
	memset(hrq->method, 0, REQ_METHOD_SIZE);
	memset(hrq->path, 0, REQ_PATH_SIZE);
	memset(hrq->http_version, 0, REQ_HTTP_VERSION_SIZE);
}

/* ##########################
            RESPONSE
   ######################### */
typedef struct {
	LIMArray *headers;
	StringView body; 
} HTTPResponse;

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

/* ########################## 
            SERVER
   ######################### */
const char *http_status_str(
	int code
) {
	switch (code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 413: return "Payload Too Large";
		case 415: return "Unsupported Media Type";
		case 422: return "Unprocessable Entity";
		case 429: return "Too Many Requests";
		case 500: return "Internal Server Error";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		default: return "Unknown";
	}
}

void send_json_response(
	int *client_fd,
	int status,
	char *error_message	
) {
	char message[RESPONSE_BUF_SIZE];
	int message_length = snprintf(
		message,
		sizeof(message),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: application/json\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		status,
		http_status_str(status),
		error_message
	);

	send(*client_fd, message, message_length, 0);
	close(*client_fd);
}

char *file_to_content_type(
	char *path
) {
	const char *ext = strrchr(path, '.');

	if (!ext) return "application/octet-stream";
	if (strcmp(ext, ".html") == 0) return "text/html";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".js") == 0) return "application/javascript";
	if (strcmp(ext, ".json") == 0) return "application/json";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
	if (strcmp(ext, ".webp") == 0) return "image/webp";
	return "application/octet-stream";
}

int send_stream_file(
	int *client_fd,
	char *path
) {
	if (strcmp(path, "/\0") == 0) {
		path = "index.html";
	}

	char buffer[CHUNK_SIZE];
	char public_path[PATH_SIZE];
	snprintf(public_path, PATH_SIZE, "public/%s", path);

	char *content_type = file_to_content_type(path);
	
	// TODO: if (the file is an image type, then open as "rb")
	char *fm = "r";
	// fm = "rb" isn't allowed?	
	if (strncmp(content_type, "image", 5) == 0) { 
		fm = "rb";
	}

	FILE *f = fopen(public_path, fm);
	if (f == NULL) {
		send_json_response(client_fd, 404, "{\"error\": \"Not Found\"}");
		return -1;
	}

	char response[CHUNK_SIZE];
	int response_len = snprintf(
		response, 
		sizeof(response), 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: close\r\n"
		"\r\n",
		content_type
	);
	send(*client_fd, response, response_len, 0);

	for (;;) {
		// -2 incase max chars available (for \r\n characters at end of chunk EOC)
		int byte_count = fread(buffer, CHAR_SIZE, CHUNK_SIZE - 2, f);
	
		if (byte_count) {	
			memcpy(buffer + byte_count, "\r\n", 2);	

			char hex_header[16];
			int hex_header_len = snprintf(
				hex_header, 
				sizeof(hex_header), 
				"%x\r\n", 
				byte_count
			);
			// send the chunk hex header	
			send(*client_fd, hex_header, hex_header_len, 0);
	
			// +2 for \r\n chars
			send(*client_fd, buffer, byte_count + 2, 0);
		}

		if (feof(f) != 0) break;
	}
	
	fclose(f);
	send(*client_fd, "0\r\n\r\n", 5, 0);
	close(*client_fd);
	return 0;
}

void extract_content_length(
	HTTPRequest *http_request,
	StringView *value
) {
	char *endptr;
	long double content_length = strtol(value->string, &endptr, 10);
	http_request->content_length = content_length;
}

int double_pass_headers(
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	//	0x20: ` `
	//	0x3A: `:`
	http_response->headers = xmalloc(sizeof(LIMArray));
	LIMArray lim_array = {0};
	*http_response->headers = lim_array; 

	/* As I see it, some values need to be stored, some need 
	to be viewed and reflected in our response block (some 
	in keys for hints later, some just as headers through 
	use of send(...)?) */

	for (int i = 0; i < http_request->headers->count; i++) {
		if (i == 0) continue;

		StringView value = {
			.string = http_request->headers->pointer[i].pointer,
			.count = http_request->headers->pointer[i].count
		};
	 	
		StringView key = split_by_delim(&value, 0x3A);
		trim_by_delim(&key, 0x20);
		trim_by_delim(&value, 0x20);	
		
		#define HDR(name) strncmp(key.string, name, key.count) == 0

		if (key.count == 0) continue;
		if (HDR("A-IM")) {}
		if (HDR("Accept")) {}
		if (HDR("Accept-Charset")) {}
		if (HDR("Accept-Datetime")) {}
		if (HDR("Accept-Encoding")) {}
		if (HDR("Accept-Language")) {}
		if (HDR("Access-Control-Request-Method")) {}
		if (HDR("Access-Control-Request-Headers")) {}
		if (HDR("Authorization")) {}	
		if (HDR("Cache-Control")) {}
		if (HDR("Connection")) {}
		if (HDR("Content-Encoding")) {}

		if (HDR("Content-Length")) {
			extract_content_length(http_request, &value);
			continue;
		}

		if (HDR("Content-MD5")) {}
		if (HDR("Content-Type")) {}
		if (HDR("Cookie")) {}
		if (HDR("Date")) {}
		if (HDR("Expect")) {}
		if (HDR("Forwarded")) {}
		if (HDR("From")) {}
		if (HDR("Host")) {}
		if (HDR("HTTP2-Settings")) {}
		if (HDR("If-Match")) {}
		if (HDR("If-Modified-Since")) {}
		if (HDR("If-None-Match")) {}
		if (HDR("If-Range")) {}
		if (HDR("If-Unmodified-Since")) {}
		if (HDR("Max-Forwards")) {}
		if (HDR("Origin")) {}
		if (HDR("Pragma")) {}
		if (HDR("Prefer")) {}
		if (HDR("Proxy-Authorization")) {}
		if (HDR("Range")) {}
		if (HDR("Referer")) {}
		if (HDR("TE")) {}
		if (HDR("Trailer")) {}
		
		if (HDR("Transfer-Encoding")) {
			continue;
		}

		if (HDR("User-Agent")) {}
		if (HDR("Upgrade")) {}
		if (HDR("Via")) {}
		if (HDR("Warning")) {}

		// non-standard
		if (HDR("Upgrade-Insecure-Requests")) {}
		if (HDR("X-Requested-With")) {}
		if (HDR("DNT")) {}
		if (HDR("X-Forwarded-For")) {}
		if (HDR("X-Forwarded-Host")) {}
		if (HDR("X-Forwarded-Proto")) {}
		if (HDR("Front-End-Https")) {}
		if (HDR("X-Http-Method-Override")) {}
		if (HDR("X-ATT-DeviceId")) {}
		if (HDR("X-Wap-Profile")) {}
		if (HDR("Proxy-Connection")) {}
		if (HDR("X-UIDH")) {}
		if (HDR("X-Csrf-Token")) {}
		if (HDR("X-Request-Id")) {}
		if (HDR("X-Correlation-Id")) {}
		if (HDR("Correlation-Id")) {}
		if (HDR("Save-Data")) {}
		if (HDR("Sec-GPC")) {}

		#undef HDR
	}

	return 0;
}

int extract_path_method_version(
	HTTPRequest *req
) {
	size_t line_size = 256;
	char *header = xmalloc(line_size);
	if (header == NULL) return -1;

	int size = snprintf(
		header,
		line_size,
		SV_fmt, 
		(int) req->headers->pointer[0].count,
		req->headers->pointer[0].pointer
	);
	
	StringView svh = sv(header);
	StringView fsvh = split_by_delim(&svh, 0x20);
	StringView path = split_by_delim(&svh, 0x20);

	SV_to_memory(req->method, REQ_METHOD_SIZE, &fsvh);
	SV_to_memory(req->path, REQ_PATH_SIZE, &path);
	SV_to_memory(req->http_version, REQ_HTTP_VERSION_SIZE, &svh);

	free(header);
	return 0;
}

LIMArray find_header_bounds(
	char *req
) {
	StringView s = sv(req);
	
	LIMArray lim_array = {0};
	int last_line = 0;
	
	for (int i = 0; i < s.count; i++) {
		if (i + 1 >= s.count) break;
	
		// Checks for \r\n (carriage return, newline)
		if (s.string[i] == 0x0D && s.string[i + 1] == 0x0A) {
			char *line_start = (last_line == 0) ? s.string : &s.string[last_line + 2];
			int count = (int)(s.string + i - line_start);
			LineInMemory l = lim(line_start, count);
	
			arr_append(lim_array, l);
			last_line = i;
		}
	}

	return lim_array;
}

int recv_chunks(
	int *client_fd,
	char *buffer,
	size_t *total,
	size_t *buffer_size
) {
	ssize_t recv_count = recv(*client_fd, buffer + *total, *buffer_size - *total - 1, 0);
	if (recv_count <= 0) return -1;
	*total += recv_count;
	return 0;
}

int recv_body_chunks(
	int *client_fd,
	char **buffer,
	size_t buffer_size
) {
	if (buffer_size <= 0 || buffer_size >= MAX_CONTENT_LENGTH)
		return -1;

	*buffer = xmalloc(buffer_size + 1);
	if (*buffer == NULL) return -1;

	size_t total = 0;

	for (;;) {
		printf("TOTAL: %ld\n", total);
		printf("BUFFER_SIZE: %ld\n", buffer_size);
		if (total >= buffer_size) break;

		int chunk_result = recv_chunks(
			client_fd,
			*buffer,
			&total,
			&buffer_size
		);

		if (chunk_result == -1) {
			if (total == 0) {
				free(*buffer);
				*buffer = NULL;
				return -1;
			}
			break;
		}
	}

	(*buffer)[total] = '\0';
	return 0;
}

int recv_header_chunks(
	int *client_fd,
	char *buffer
) {
	ssize_t nn_count = 0;
	size_t max_header_size = CLIENT_BUF_SIZE;

	for (;;) {   
		if (recv_chunks(
			client_fd,
			buffer,
			&nn_count,
			&max_header_size
		) == -1) return -1;

		char *mmp = memmem(buffer, nn_count, "\r\n\r\n", 4);
		
		// Found the header terminator
		if (mmp != NULL) break;
	}

	// Add null terminator to end of string
	buffer[nn_count] = '\0';
	return 0;
}

int fill_http_request(
	int *client_fd,
	HTTPRequest *req
) {
	// Receive chunks until the body \r\n\r\n
	char *headers = xmalloc(CLIENT_BUF_SIZE);
	int rhc_success = recv_header_chunks(client_fd, headers);
	if (rhc_success == -1) {
		printf("Failed to receive request.\n");
		return -1;
	}

	LIMArray lim_array = find_header_bounds(headers);
	if (lim_array.count == 0) {
		printf("Failed to find any header info.\n");
		return -1;
	}

	req->headers = xmalloc(sizeof(LIMArray));
	if (req->headers == NULL) return -1;

	*req->headers = lim_array;
	return extract_path_method_version(req);
}

void request_response_cycle(
	int sfd,
	struct sockaddr * restrict peer_addr,
	socklen_t *peer_addrlen
) {
	struct timeval tv = { 
		.tv_sec = 0,
		.tv_usec = 500000
	}; // 50ms
	int tv_size = sizeof(tv);
/*
	struct pollfd fds[1];
	fds[0].fd = *client_fd;
	fds[0].events = POLLIN;
	nfds_t nfds = 1;

	if (poll(fds, nfds, 250) < 0) {
		printf("Closed (%d) with poll(...)\n", *client_fd);
		return NULL; 
	}
*/


	// REQUEST / RESPONSE CYCLE
	for (;;) {
		// Try to accept a connection request from the client.
		int client_fd = accept(sfd, peer_addr, peer_addrlen);
		if (client_fd == -1) {
			printf("Failed to accept connection from client.\n");
			continue;
		}

		// Set a default timeout on the client sending bytes to server.
		setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, tv_size);

		HTTPRequest http_request = {0};
		HTTPResponse http_response = {0};

		#define ERROR() send_json_response(&client_fd, 400, "{\"error\": \"Bad Request\"");
		#define FREE(req, res) { \
			ERROR(); \
			freeHTTPRequest((req)); \
			freeHTTPResponse((res)); \
			continue; \
		} \
	
		if (fill_http_request(&client_fd, &http_request) == -1) {
			FREE(&http_request, &http_response);
		}	

		if (double_pass_headers(&http_request, &http_response) == -1) {
			FREE(&http_request, &http_response);
		}

		// POST, PUT, PATCH
		if (strcmp(http_request.method, "POST") == 0) {
			if (recv_body_chunks(
				&client_fd,
				&http_request.body,
				(size_t) http_request.content_length	
			) == -1) FREE(&http_request, &http_response);
			printf("BODY: %s\n", http_request.body);
		}

		send_stream_file(&client_fd, http_request.path);

		freeHTTPRequest(&http_request);
		freeHTTPResponse(&http_response); 
	}
}

/* ########################## 
         START SERVER
   ######################### */
int initiate_server(
	struct addrinfo *h,
	int *sfd,
	char *port
) {
	ssize_t nread;
	struct addrinfo *result = {0}, *rp = {0};
	int s;

	s = getaddrinfo(NULL, port, h, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		*sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (*sfd == -1) continue; // failed, try again

		int bs = bind(*sfd, rp->ai_addr, rp->ai_addrlen);
		if (bs == 0) break; // success, stop
		
		close(*sfd);
	} 

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	return 0;
}

int tcp_server(
	char *port
) {
	struct addrinfo h = {0};
	int sfd;
	socklen_t peer_addrlen = sizeof(struct sockaddr_storage);
	struct sockaddr_storage peer_addr = {0};

	memset(&h, 0, sizeof(h));
	h.ai_flags = AI_PASSIVE; // int
	h.ai_family = AF_UNSPEC; // int
	h.ai_socktype = SOCK_STREAM; // int
	h.ai_protocol = 0; // int
	h.ai_addr = NULL; // struct sockaddr
	h.ai_canonname = NULL; // char
	h.ai_next = NULL; // struct addrinfo

	if (initiate_server(&h, &sfd, port) == -1)
		return -1;

	// param 2 = backlog; ...how many requests can queue up before ECONNREFUSED or manual queueing.
	if (listen(sfd, 50) == -1) {
		printf("Failed to listen.\n");
		return -1;
	}
	
	printf("Server listening on port %s\n", port);
	request_response_cycle(sfd, (struct sockaddr*)&peer_addr, &peer_addrlen);
	return 0;
}

int main(
	int argc, 
	char **argv
) {	
	if (argc < 2) {
		printf("Command failed, use:\n\nserver <PORT>\n");
		return 1;
	}

	if (tcp_server(argv[1]) == -1) 
		exit(EXIT_FAILURE);

	printf("Process exited cleanly.");	
	return 0;
}
