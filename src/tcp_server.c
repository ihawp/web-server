#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include "tcp_server.h"

int tcp_server(
	char *port
) {
	int sfd;
	struct addrinfo h = {0};
	ssize_t nread;
	struct addrinfo *result = {0}, *rp = {0};
	int s, opt = 1, opt_size = sizeof(opt), bs;

	memset(&h, 0, sizeof(h));
	h.ai_flags = AI_PASSIVE; // int
	h.ai_family = AF_UNSPEC; // int
	h.ai_socktype = SOCK_STREAM; // int
	h.ai_protocol = 0; // int
	h.ai_addr = NULL; // struct sockaddr
	h.ai_canonname = NULL; // char
	h.ai_next = NULL; // struct addrinfo

	s = getaddrinfo(NULL, port, &h, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) continue; // failed, try again

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &opt, opt_size) == -1) {
			perror("setsockopt SO_REUSEPORT");
			exit(EXIT_FAILURE);
		}

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, opt_size) == -1) {
			perror("setsockopt SO_REUSEADDR");
			exit(EXIT_FAILURE);
		}

		bs = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		if (bs == 0) break;
		
		close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	return sfd;
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

int setnonblocking(
	int fd
) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}