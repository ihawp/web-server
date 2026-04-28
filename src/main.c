#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include "string_view.h"
#include "line_in_memory_array.h"
#include "program_speed.h"
#include "http_request.h"
#include "http_response.h"
#include "xmalloc.h"
#include "array_append.h"
#include "data.h"

// TODO: Organize and label better
#define CLIENT_BUF_SIZE 2048
#define CHUNK_SIZE 512
#define RESPONSE_BUF_SIZE 512
#define PATH_SIZE 512
#define CHAR_SIZE sizeof(char)
#define MAX_CONTENT_LENGTH 4096

// epoll/threads
#define MAX_EVENTS 10
#define MAX_WORKERS 3

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


// This should accept an already open and tested fd, rather than have the 404 loop if f == null and what not.
int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	if (strcmp(http_request->path, "/\0") == 0) {
		strcpy(http_request->path, "index.html");
	}

	char buffer[CHUNK_SIZE];
	char public_path[PATH_SIZE];
	snprintf(public_path, PATH_SIZE, "public/%s", http_request->path);

	char *content_type = file_to_content_type(http_request->path);
	
	char *fm = "r";
	if (strncmp(content_type, "image", 5) == 0) { 
		fm = "rb";
	}

	FILE *f = fopen(public_path, fm);
	if (f == NULL) {
		send_json_response(client_fd, 400, "{\"error\": \"fill_http_request handle_request\", \"success\": false}");
		return -1;
	}

	http_response->status = 200;

	char response[CHUNK_SIZE];
	int response_len = snprintf(
		response, 
		sizeof(response), 
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: keep-alive\r\n"
		"\r\n",
		http_response->status,
		http_status_str(http_response->status),
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
	shutdown(*client_fd, SHUT_WR);
	return 0;
}

int parse_headers(
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
			char *endptr;
			long content_length = strtol(value.string, &endptr, 10); // convert to base 10
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
	char *header = xmalloc(req->headers->pointer[0].count + 1);
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
	ssize_t recv_count = recv(*client_fd, buffer + *total, *buffer_size - *total, 0);
    if (recv_count <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        return -1;
    }
	*total += recv_count;
	return 0;
}

// TODO: post requests get stuck in here, SEE "oh noes"
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
		if (total >= buffer_size) break;

		int chunk_result = recv_chunks(
			client_fd,
			*buffer,
			&total,
			&buffer_size
		);

		printf("Chunk Result: %d\n", chunk_result);

		if (chunk_result == 0) break;
		if (chunk_result == 1) continue;
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

		int r = recv_chunks(client_fd, buffer, &nn_count, &max_header_size);
		if (r == -1) return -1;
		if (r == 1) continue;
		char *mmp = memmem(buffer, nn_count, "\r\n\r\n", 4);
		
		// Found the header terminator
		if (mmp != NULL) break;
	}

	// Add null terminator to end of string
	buffer[nn_count] = '\0';
	return 0;
}

int capture_headers(
	int *client_fd,
	HTTPRequest *req
) {
	// Receive chunks until the body \r\n\r\n
	char *headers = xmalloc(CLIENT_BUF_SIZE);
	if (recv_header_chunks(client_fd, headers) == -1) {
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

void handle_request(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {

	// create array of pointers/count for each header from buffer
	if (capture_headers(client_fd, http_request) == -1) {
		send_json_response(client_fd, 400, "{\"error\": \"fill_http_request handle_request\", \"success\": false}");
		return;
	}

	// review received headers
	if (parse_headers(http_request, http_response) == -1) {
		send_json_response(client_fd, 400, "{\"error\": \"double_pass_headers handle_request\", \"success\": false}");
		return;
	}

	if (strcmp(http_request->method, "POST") == 0) {
		if (recv_body_chunks(client_fd, &http_request->body, (size_t) http_request->content_length) == -1) {
			send_json_response(client_fd, 400, "{\"error\": \"recv_body_chunks handle_request\", \"success\": false}");
			return;
		}
		
		printf("BODY: %s\n", http_request->body);
	}

	if (strcmp(http_request->method, "GET") == 0) {
		// handles error
		send_stream_file(client_fd, http_request, http_response);
		return;
	}

	// error, could send 404.html instead
	send_json_response(client_fd, 400, "{\"error\": \"NO METHOD handle_request\", \"success\": false}");
}

int setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void *worker(
	void *data
) {
	WorkerData *wd = data;
	HTTPRequest http_request = {0};
	HTTPResponse http_response = {0};
	struct sockaddr_storage peer_addr = {0};

	socklen_t peer_addrlen = sizeof(struct sockaddr_storage);
	int client_fd, n, ectl, result;
	struct epoll_event ev, events[MAX_EVENTS];
	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

	for (;;) {
		// if nothing, continue;
		result = epoll_wait(wd->epc, events, MAX_EVENTS, -1);
		if (result == -1) continue;

		for (n = 0; n < result; ++n) {
			if (events[n].data.fd == wd->sfd) {
				
				client_fd = accept(wd->sfd, (struct sockaddr*) &peer_addr, (socklen_t*) &peer_addrlen);
				if (client_fd == -1) {
					printf("client_fd\n");
					continue;
				};

				if (setnonblocking(client_fd) == -1) {
					printf("blocking\n");
				}

				ev.data.fd = client_fd;

				ectl = epoll_ctl(wd->epc, EPOLL_CTL_ADD, client_fd, &ev);

				if (errno == EEXIST) {
					printf("exist\n");
				};

				if (ectl == -1) {
					perror("Failed to epoll_ctl client_fd\n");
					exit(EXIT_FAILURE);
				}
			} else {
				int fd = events[n].data.fd;

				memset(&http_request, 0, sizeof(http_request));
				memset(&http_response, 0, sizeof(http_response));

				handle_request(&fd, &http_request, &http_response);

				freeHTTPRequest(&http_request);
				freeHTTPResponse(&http_response);

				epoll_ctl(wd->epc, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
			}
		}
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
	int s, opt = 1, opt_size = sizeof(opt), bs;

	s = getaddrinfo(NULL, port, h, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		*sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (*sfd == -1) continue; // failed, try again

		if (setsockopt(*sfd, SOL_SOCKET, SO_REUSEPORT, &opt, opt_size) == -1) {
			perror("setsockopt SO_REUSEPORT");
			exit(EXIT_FAILURE);
		}

		if (setsockopt(*sfd, SOL_SOCKET, SO_REUSEADDR, &opt, opt_size) == -1) {
			perror("setsockopt SO_REUSEADDR");
			exit(EXIT_FAILURE);
		}

		bs = bind(*sfd, rp->ai_addr, rp->ai_addrlen);
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
	int sfd;
	struct addrinfo h = {0};

	memset(&h, 0, sizeof(h));
	h.ai_flags = AI_PASSIVE; // int
	h.ai_family = AF_UNSPEC; // int
	h.ai_socktype = SOCK_STREAM; // int
	h.ai_protocol = 0; // int
	h.ai_addr = NULL; // struct sockaddr
	h.ai_canonname = NULL; // char
	h.ai_next = NULL; // struct addrinfo

	if (initiate_server(&h, &sfd, port) == -1) {
		return -1;
	}

	return sfd;
}

int main(
	int argc,
	char **argv
) {
	char *port;
	int sfd, epc, result, client_fd, i, n, ectl;
	struct epoll_event ev, events[MAX_EVENTS];
	pthread_t workers[MAX_WORKERS];
	WorkerData data = {0};

	if (argc < 2) {
		printf("Server startup failed, try:\n\n<Server Name> <PORT>\n");
		exit(1);
	}

	port = argv[1];
	sfd = tcp_server(port);
	if (sfd == -1) exit(EXIT_FAILURE);

	if (listen(sfd, SOMAXCONN) == -1) {
		printf("Failed to listen.\n");
		exit(EXIT_FAILURE);
	}
	printf("Server listening on port %s\n", port);

	epc = epoll_create1(0);
	ev.events = EPOLLIN | EPOLLEXCLUSIVE;
	ev.data.fd = sfd;
	if (epoll_ctl(epc, EPOLL_CTL_ADD, sfd, &ev) == -1) {
		printf("sfd epoll_ctl\n");
		exit(EXIT_FAILURE);
	}

	data.epc = epc;
	data.sfd = sfd;
	pthread_mutex_init(&data.lock, NULL);
    pthread_cond_init(&data.ready, NULL);
	for (i = 0; i < MAX_WORKERS; i++) {
		pthread_create(&workers[i], NULL, (void*) worker, &data);
	}

	pause();
}