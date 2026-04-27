#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
// #include <sys/epoll.h>
#include "string_view.h" // use `gcc -iquote headers/` to include all headers
#include "line_in_memory_array.h"
#include "program_speed.h"
#include "http_request.h"
#include "http_response.h"
#include "xmalloc.h"
#include "array_append.h"

// TODO: Organize and label better
#define CLIENT_BUF_SIZE 2048
#define CHUNK_SIZE 512
#define RESPONSE_BUF_SIZE 512
#define PATH_SIZE 256
#define CHAR_SIZE sizeof(char)
#define MAX_CONTENT_LENGTH 4096

// HEADERS

const char *http_status_str(
	int code
);

void send_json_response(
	int *client_fd,
	int status,
	char *error_message	
);

int send_stream_file(
	int *client_fd,
	char *path
);

int double_pass_headers(
	HTTPRequest *http_request,
	HTTPResponse *http_response
);

char *file_to_content_type(
	char *path
);

int extract_path_method_version(
	HTTPRequest *req
);

int recv_chunks(
	int *client_fd,
	char *buffer,
	size_t *total,
	size_t *buffer_size
);

int recv_body_chunks(
	int *client_fd,
	char **buffer,
	size_t buffer_size
);

int recv_header_chunks(
	int *client_fd,
	char *buffer
);

int fill_http_request(
	int *client_fd,
	HTTPRequest *req
);

void request_response_cycle(
	int sfd,
	struct sockaddr * restrict peer_addr,
	socklen_t *peer_addrlen
);

int initiate_server(
	struct addrinfo *h,
	int *sfd,
	char *port
);

int tcp_server(
	char *port
);

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
	
	char *fm = "r";
	if (strncmp(content_type, "image", 5) == 0) { 
		fm = "rb";
	}

	FILE *f = fopen(public_path, fm);
	if (f == NULL) {

		// TODO: remove
		srand(time(NULL));
		int random = rand() % 10;

		if (random > 5) {
			send_json_response(client_fd, 404, "{\"error\": \"Not Found\", \"success\": false}");
		} else {
			send_json_response(client_fd, 200, "{\"message\": \"Found it!\", \"success\": true}");
		}

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

int double_pass_headers(
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	//	0x20: ` `
	//	0x3A: `:`
	http_response->headers = xmalloc(sizeof(LIMArray));
	LIMArray lim_array = {0};
	*http_response->headers = lim_array; 

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
		if (HDR("Accept")) {
			SV_print(&value);
			continue;
		}
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
			char *endptr;
			long double content_length = strtol(value.string, &endptr, 10); // convert to base 10
			http_request->content_length = content_length;
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
		if (HDR("Transfer-Encoding")) {}
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
	char *header = xmalloc(req->headers->pointer[0].count);
	if (header == NULL) return -1;

	int size = snprintf(
		header,
		req->headers->pointer[0].count + 1,
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
	if (buffer_size <= 0 || buffer_size >= MAX_CONTENT_LENGTH) {
		return -1;
	}

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

#define ERROR() send_json_response(&client_fd, 400, "{\"error\": \"Bad Request\", \"success\": false}");
#define FREE(req, res, speed) { \
	ERROR(); \
	freeHTTPRequest((req)); \
	freeHTTPResponse((res)); \
	end((speed)); \
	continue; \
} \

void request_response_cycle(
	int sfd,
	struct sockaddr * restrict peer_addr,
	socklen_t *peer_addrlen
) {
	for (;;) {
		int client_fd = accept(sfd, peer_addr, peer_addrlen);
		if (client_fd == -1) continue;

		ProgramSpeed speed;
		start(&speed);

		HTTPRequest http_request = {0};
		HTTPResponse http_response = {0};

		if (fill_http_request(&client_fd, &http_request) == -1) {
			FREE(&http_request, &http_response, &speed);
		}	

		if (double_pass_headers(&http_request, &http_response) == -1) {
			FREE(&http_request, &http_response, &speed);
		}

		// POST, PUT, PATCH
		if (strcmp(http_request.method, "POST") == 0) {
			if (recv_body_chunks(
				&client_fd,
				&http_request.body,
				(size_t) http_request.content_length	
			) == -1) FREE(&http_request, &http_response, &speed);
			
			printf("BODY: %s\n", http_request.body);
		}

		send_stream_file(&client_fd, http_request.path);

		freeHTTPRequest(&http_request);
		freeHTTPResponse(&http_response);
		
		end(&speed);
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
	struct timeval tv = {
		.tv_sec = 0,
		.tv_usec = 50000
	}; // 50ms
	int tv_size = sizeof(tv);

	s = getaddrinfo(NULL, port, h, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		*sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (*sfd == -1) continue; // failed, try again

		setsockopt(*sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, tv_size);

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

	// create thread for listenning?
	// create 

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
		printf("Server startup failed, try:\n\n<Server Name> <PORT>\n");
		return 1;
	}

	// this is useless usecase
	/*

	pthread_t thread_tcp_server;
	pthread_create(&thread_tcp_server, NULL, tcp_server, NULL);
	
	*/
	
	if (tcp_server(argv[1]) == -1) { 
		exit(EXIT_FAILURE);
	}

	return 0;
}
