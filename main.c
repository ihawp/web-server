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

/*

HTTP/1.1 Server

TODO:
- Stream client requests, right now I am pretty sure we just read CLIENT_BUF_SIZE and move on
- Request header parsing
- Routing
- Custom headers per response

*/

int faddlots(
	char *filename,
    	int length,	
	char *text[]
) {
	FILE *f = fopen(filename, "a");

	if (f == NULL) return -1;

	for (int i = 0; i < length; i++) {
		fprintf(f, "%s", text[i]);
		if (i == length - 1) fprintf(f, "\n");
	}

	fclose(f);

	return 0;
}

int fadd(
	char *filename,
	char *text
) {
	FILE *f = fopen(filename, "a");

	if (f == NULL) {
		printf("FILE ERROR: %s", strerror(errno));
		return -1;
	}

	fprintf(f, "%s\n", text);
	return fclose(f);
}

void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		printf("Failed to allocate memory.");
		return 0;
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
	if (sv->count == 0) return
;	if (amount > sv->count) amount = sv->count;
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

void trim(
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

	char path[256];
	snprintf(path, sizeof(path), "public/%s", filename);

	FILE *f = fopen(path, "r");
	if (f == NULL) {
		printf("Failed to open file: %s\n", filename);
		return;
	}

	for (;;) {
		// -2 incase max chars available (for \r\n characters at end of chunk EOC)
		int byte_count = fread(buffer, sizeof(char), CHUNK_SIZE - 2, f);
	
		if (byte_count) {	
			memcpy(buffer + byte_count, "\r\n", 2);	

			char hex_header[16];
			int hex_header_len = snprintf(hex_header, sizeof(hex_header), "%x\r\n", byte_count);
			
			send(client_fd, hex_header, hex_header_len, 0); // send the chunk hex header
			send(client_fd, buffer, byte_count + 2, 0); // +2 for \r\n chars
		}

		// if EOF (end of file) then send the terminating ASCII 0 literal	
		if (feof(f) != 0) {
			send(client_fd, "0\r\n\r\n", 5, 0);
			break;
		}
	}

	fclose(f);
}

void accept_tcp_connections(
	int sfd,
	struct sockaddr * restrict peer_addr,
	socklen_t *peer_addrlen
) {
	for (;;) {
		*peer_addrlen = sizeof(struct sockaddr_storage);
		int client_fd = accept(sfd, peer_addr, peer_addrlen); // pass NULL, NULL as latter to not store client

		printf("sa_family:\t\t%d\n", peer_addr->sa_family);
		printf("addrlen:\t\t%d\n", *peer_addrlen);

		// ntohs
		// inet_ntop (ipv4), inet_ntop6 (ipv6)

		if (client_fd == -1) {
			printf("Failed to accept connection from client.\n");
			continue;
		}

		char buffer[CLIENT_BUF_SIZE];
		ssize_t n = recv(client_fd, buffer, CLIENT_BUF_SIZE, 0);

		printf("Client Request Buffer:\n%s\n", buffer);

		// stream the client request, begin parsing
		// parse the client request: figure out what they want
		// send it to them

		char response[CHUNK_SIZE];
		int response_len = snprintf(
			response, 
			sizeof(response), 
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			// "Content-Length: %zu\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n"
		);

		send(client_fd, response, response_len, 0);

		char *filename = "index.html";
		send_stream_file(client_fd, filename);
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

typedef struct {
	char *fy;
} TCP_Options;

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
		printf("Failed to listen.");
		exit(EXIT_FAILURE);
	}
	printf("TCP Connection listenning on port %s\n", port);
	
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
