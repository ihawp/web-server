#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <stdarg.h>

#include "http.h"
#include "line_in_memory_array.h"
#include "program_speed.h"
#include "helpers.h"
#include "tcp_server.h"
#include "process_data.h"

void free_http_response(
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
	char *new_header = xmalloc(HEADER_SIZE);
	if (new_header == NULL) return -1;
	LineInMemory new_lim = lim(new_header, HEADER_SIZE);
	arr_append((*htr->headers), new_lim);
	return 0;
}

void free_http_request(
	HTTPRequest *hrq
) {
	freelima(hrq->headers);
	memset(hrq->method, 0, REQ_METHOD_SIZE);
	memset(hrq->path, 0, REQ_PATH_SIZE);
	memset(hrq->http_version, 0, REQ_HTTP_VERSION_SIZE);
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

// This should accept an already open and tested fd, rather than have the 404 loop if f == null and what not.
int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	if (strcmp(http_request->path, "/") == 0) {
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
		// -2 for trailing \r\n
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

			send(*client_fd, hex_header, hex_header_len, 0);
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

		if (key.count == 0) continue;

		trim_by_delim(&key, 0x20);
		trim_by_delim(&value, 0x20);

		// capture value from content-length header
		if (strncmp(key.string, "Content-Length", key.count) == 0) {
			char *endptr;
			long content_length = strtol(value.string, &endptr, 10); // convert to base 10
			http_request->content_length = content_length;
			continue;
		}
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

char *recv_header_chunks(
	int *client_fd,
	char *buffer,
	ssize_t *recv_count
) {
	size_t max_header_size = CLIENT_BUF_SIZE - 1;
	int status;
	char *mmp;

	for (;;) {   
		status = recv_chunks(client_fd, buffer, recv_count, &max_header_size);
		if (status == -1 || status == 2) return NULL;
		if (status == 1) continue;
		mmp = memmem(buffer, *recv_count, "\r\n\r\n", 4); // memmem only finds \r\n\r\n in bytes so I need to pinpoint it
		if (mmp != NULL) {
			buffer[*recv_count] = '\0';
			return mmp;
		}
	}

	return NULL;
}

int recv_body_chunks(
	int *client_fd,
	char *buffer,
	size_t content_length,
	ssize_t *total,
	size_t *body_length
) {
	// DO NOT ASSIGN THE BUFFER INSIDE THIS FUNCTION
	for (;;) {
		if (*body_length >= content_length) {
			printf("breaking on >=\n");
			break;
		}

		int status = recv_chunks(
			client_fd,
			buffer,
			body_length,
			&content_length
		);

		if (status == 1) continue; // keeps looping on 1 because EAGAIN or EWOULDBLOCK is set
		if (status == -1 || status == 2) {
			if (*body_length == 0) {
				free(buffer);
				buffer = NULL;
				return -1;
			}
			break;
		}
	}

	buffer[*body_length] = '\0';
	return 0;
}

int handle_request(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	char *headers = xmalloc(CLIENT_BUF_SIZE), *body_start;
	ssize_t recv_count = 0;
	size_t bs_size, body_length;

	// instead we create the buffer for the httprequest->body here
	if (headers == NULL) {
		printf("Failed to allocate memory for body\n");
		return -1;
	}

	body_start = recv_header_chunks(client_fd, headers, &recv_count);
	if (body_start == NULL) {
		printf("mmp failed\n");
		return -1;
	}

	// ths bs_size tells us how many characters the actual headers are
	// the recv_count - bs_size = the length of body
	// already added, which will be used as the 3rd parameter of memmove
	bs_size = body_start - headers;
	body_length = recv_count - bs_size - 4; // remove 4 for \r\n\r\n
	*body_start = '\0'; // add a null terminator for end of headers
	body_start += 4; // move past \r\n\r\n to start of body content

	LIMArray lim_array = find_header_bounds(headers);
	if (lim_array.count == 0) {
		printf("Failed to find any header info.\n");
		return -1;
	}

	http_request->headers = xmalloc(sizeof(LIMArray));
	if (http_request->headers == NULL) return -1;
	*http_request->headers = lim_array;
	
	if (extract_path_method_version(http_request) == -1) {
		printf("Failed to extract_path_method_version\n");
		return -1;
	}

	if (parse_headers(http_request, http_response) == -1) {
		printf("Failed to parse headers\n");
		return -1;
	};

	if (strcmp(http_request->method, "POST") == 0) {
		if (http_request->content_length >= MAX_CONTENT_LENGTH - CLIENT_BUF_SIZE) {
			return -1; // can start making custom error codes #define OVER_LIMIT 10
		}

		if (body_length == http_request->content_length) {
			printf("whole body found in first recv\n");
		}

		http_request->body = xmalloc(http_request->content_length + 1);
		if (http_request->body == NULL) return -1;

		// move the originally (potentially) captured body content
		// into the proper body container: http_request->body
		memcpy(http_request->body, body_start, http_request->content_length); // bs_size is greater than content length

		if (recv_body_chunks(
			client_fd, 
			http_request->body, 
			(size_t) http_request->content_length, 
			&recv_count,
			&body_length
		) == -1) {
			printf("Failed to recieve body chunks\n");
			return -1;
		}

		printf("BODY LENGTH OUTSIDE: %ld\nRECV_COUNT OUTSIDE: %ld\n", body_length, recv_count);
		printf("BODY: %s\nBODY STR LEN: %ld\n", http_request->body, strlen(http_request->body));
		send_json_response(client_fd, 200, "{\"success\": true, \"message\": \"We recieved your data!\"}");
		
		return 0;
	}

	if (strcmp(http_request->method, "GET") == 0) {
		if (send_stream_file(client_fd, http_request, http_response) == -1) {
			return -1;
		}
		
		return 0;
	}

	return -1;
}

void *http_worker(
	void *data
) {
	struct process_data *wd = data;
	HTTPRequest http_request = {0};
	HTTPResponse http_response = {0};
	struct sockaddr_storage peer_addr = {0};
	struct epoll_event ev, events[MAX_EVENTS] = {0};
	socklen_t peer_addrlen = sizeof(struct sockaddr_storage);
	int client_fd, n, ectl, result, fd;
	pid_t tid;
	struct program_speed speed = {0};

	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	tid = syscall(SYS_gettid);
	int *tid_p = &tid;

	printfid("Worker Started", tid);

	for (;;) {
		result = epoll_wait(wd->epc, events, MAX_EVENTS, -1);
		if (result == -1) continue;

		for (n = 0; n < result; ++n) {
			if (events[n].data.fd == wd->sfd) {
				
				client_fd = accept(wd->sfd, (struct sockaddr*) &peer_addr, (socklen_t*) &peer_addrlen);
				if (client_fd == -1) {
					printfid("client_fd", tid);
					continue;
				}

				if (setnonblocking(client_fd) == -1) {
					printfid("blocking", tid);
				}

				ev.data.fd = client_fd;
				ectl = epoll_ctl(wd->epc, EPOLL_CTL_ADD, client_fd, &ev);
				
				if (errno == EEXIST) {
					printfid("EEXIST", tid);
				}

				if (ectl == -1) {
					printfid("setnonblocking epoll_ctl", tid);
					exit(EXIT_FAILURE);
				}
			} else {
				fd = events[n].data.fd;

				memset(&http_request, 0, sizeof(http_request));
				memset(&http_response, 0, sizeof(http_response));

				ps_cap(&speed.start);
				// ps_print_pit(&speed.start, tid_p);
				
				if (handle_request(&fd, &http_request, &http_response) == -1) {
					send_json_response(&fd, 200, "{\"error\": \"Failed to handle request\", \"success\": false}");
					printfid("handle_request", tid);
				}

				ps_cap(&speed.end);
				// ps_print_pit(&speed.end, tid_p);
				ps_print_elapsed(&speed, tid_p);

				free_http_request(&http_request);
				free_http_response(&http_response);
				epoll_ctl(wd->epc, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
			}
		}
	}

	printfid("Worker Exiting", tid);
}

void http_server(
	int sfd,
	char *port
) {
	int epc, result, client_fd, i, n, ectl;
	struct epoll_event ev, events[MAX_EVENTS] = {0};
	pthread_t workers[MAX_WORKERS];
	struct process_data data = {0};

	data.pid = getpid();

	if (listen(sfd, SOMAXCONN) == -1) {
		printfid("Failed to listen", data.pid);
		exit(EXIT_FAILURE);
	}
	
	printf("\e[1;1H\e[2J");
	printfid("Server listening on port %s", data.pid, port);

	epc = epoll_create1(0);
	ev.events = EPOLLIN | EPOLLEXCLUSIVE;
	ev.data.fd = sfd;
	if (epoll_ctl(epc, EPOLL_CTL_ADD, sfd, &ev) == -1) {
		printfid("sfd epoll_ctl", data.pid);
		exit(EXIT_FAILURE);
	}

	data.epc = epc;
	data.sfd = sfd;
	pthread_mutex_init(&data.lock, NULL);
    pthread_cond_init(&data.ready, NULL);

	for (i = 0; i < MAX_WORKERS; i++) {
		pthread_create(&workers[i], NULL, (void*) http_worker, &data);
	}
}