#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define CLIENT_BUF_SIZE 512
#define CHUNK_SIZE 512
#define RESPONSE_BUF_SIZE 512
#define PATH_SIZE 256
#define CHAR_SIZE sizeof(char)

/*

HTTP/1.1 Server

TODO:
- Stream client requests (and don't trust POST requests :()
- Request header parsing
- Routing
- Custom headers per response

*/

void *xmalloc(size_t size) {
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

StringView sv(char *string) {
	return (StringView) {
		.string = string,
		.count = strlen(string) 
	};
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
	char *delim
) {
	while (sv->string[0] == *delim) {
		sv->string += 1;
		sv->count -= 1;
	}	
}

void delim_from_right(
	StringView *sv,
	char *delim
) {
	while (sv->string[sv->count - 1] == *delim) {
		sv->count -= 1;
	}
}

void trim_by_delim(
	StringView *sv,
	char *delim
) {
	delim_from_left(sv, delim);
	delim_from_right(sv, delim);
}

/* ########################## 
            SERVER
   ######################### */
void send_stream_file(
	int client_fd,
	char *filename
) {

	char buffer[CHUNK_SIZE];

	char path[PATH_SIZE];
	snprintf(path, PATH_SIZE, "public/%s", filename);

	FILE *f = fopen(path, "r");
	
	if (f == NULL) {
		printf("Failed to open file: %s\n", filename);
		send(client_fd, "0\r\n\r\n", 5, 0);
		return;
	}

	// Figure out a better way to set and send headers (which you will have to from parsing better)
	char response[CHUNK_SIZE];
	int response_len = snprintf(
		response, 
		sizeof(response), 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: close\r\n"
		"\r\n"
	);
	send(client_fd, response, response_len, 0);

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
			
			send(client_fd, hex_header, hex_header_len, 0); // send the chunk hex header
			send(client_fd, buffer, byte_count + 2, 0); // +2 for \r\n chars
		}

		// if EOF (end of file) then send the terminating ASCII 0 literal	
		if (feof(f) != 0) break;
	}
	
	send(client_fd, "0\r\n\r\n", 5, 0);
	fclose(f);
}

// contains a pointer and a count
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

// returning (arr) is lazy? it expects type passed as arr as return (right now that is LIMArray)
#define arr_append(arr, item) { \
	if ((arr).count + 1 >= (arr).capacity) { \
		if ((arr).capacity == 0) (arr).capacity = 256; \
		size_t new_alloc_amount = (arr).capacity * 2 * sizeof(*(arr).pointer); \
		void *new = realloc((arr).pointer, new_alloc_amount); \
		if (new == NULL) { \
			printf("Failed to reallocate\n"); \
			return (arr); \
		} \
		(arr).pointer = new; \
		(arr).capacity *= 2; \
	} \
	(arr).count += 1; \
	(arr).pointer[(arr).count - 1] = item; \
} \

LIMArray parse_request_headers(
	char *req
) {
	StringView s = sv(req);
	LIMArray lim_array = lima(NULL, 0, 0);
	int last_line = 0;
	
	for (int i = 0; i < s.count; i++) {
		unsigned char first;
		unsigned char second;

		if (i + 1 >= s.count) break;
		// Checks for \r\n (carriage return, newline)
		if (s.string[i] == 0x0D && s.string[i + 1] == 0x0A) {
			LineInMemory l = lim(
				&s.string[i],
				i - last_line - 2 // 2 for \r\n?
			);
			arr_append(lim_array, l);
			last_line = i;
		}
	}

	return lim_array;
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

// works as json response function
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

void accept_tcp_connections(
	int sfd,
	struct sockaddr * restrict peer_addr,
	socklen_t *peer_addrlen
) {

	*peer_addrlen = sizeof(struct sockaddr_storage);

	// REQUEST / RESPONSE CYCLE
	for (;;) {
		int client_fd = accept(sfd, peer_addr, peer_addrlen);

		if (client_fd == -1) {
			printf("Failed to accept connection from client.\n");
			continue;
		}

		// should loop
		char req[CLIENT_BUF_SIZE];
		ssize_t nn = recv(client_fd, req, CLIENT_BUF_SIZE - 1, 0);
		if (nn == -1) {
			send_json_response(&client_fd, 400, "{\"error\": \"bad request\"}");
			continue;	
		}
		// place after bytes with [nn], not at end of buffer
		req[nn] = '\0';

		LIMArray lim_array = parse_request_headers(req);
		
		if (lim_array.count == 0) {
			printf("Failed to find any header info.\n");
			send_json_response(&client_fd, 400, "{\"error\": \"bad request\"}");
			freelima(&lim_array);
			continue;
		}

		freelima(&lim_array);

		// return generic index.html for all routes
		send_stream_file(client_fd, "index.html");
		
		close(client_fd);
	}
}

int initiate_server(
	struct addrinfo *h,
	int *sfd,
	char *port
) {
	ssize_t nread;
	struct addrinfo *result, *rp;
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

void tcp_server(
	char *port
) {
	struct addrinfo h;
	int sfd;
	socklen_t peer_addrlen;
	struct sockaddr_storage peer_addr;

	memset(&h, 0, sizeof(h));
	h.ai_flags = AI_PASSIVE; // int
	h.ai_family = AF_UNSPEC; // int
	h.ai_socktype = SOCK_STREAM; // int
	h.ai_protocol = 0; // int
	h.ai_addr = NULL; // struct sockaddr
	h.ai_canonname = NULL; // char
	h.ai_next = NULL; // struct addrinfo

	if (initiate_server(&h, &sfd, port) == -1)
		exit(EXIT_FAILURE);

	// param 2 = backlog; ...how many requests can queue up before ECONNREFUSED or manual queueing.
	if (listen(sfd, 50) == -1) {
		printf("Failed to listen.\n");
		exit(EXIT_FAILURE);
	}
	printf("Server listening on port %s\n", port);
	
	accept_tcp_connections(sfd, (struct sockaddr*)&peer_addr, &peer_addrlen);
}

/* ########################## 
         YOU FOUND IT!
   ######################### */
int main(int argc, char **argv) {
	
	if (argc < 2) {
		printf("Too few arguments.\n");
		return 1;
	}

	tcp_server(argv[1]);
	
	return 0;
}
