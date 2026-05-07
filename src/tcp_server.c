#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include "tcp_server.h"

/*
	binds a socket to a port (of your choice)

	returns the socket file descriptor of the bound socket
*/
int tcp_server(
	char *port
) {
	int sfd;
	struct addrinfo h = {0};
	struct addrinfo *result = NULL, *rp = NULL;
	int s, opt = 1, bs;
	socklen_t opt_size = sizeof(opt);

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

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		
		close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	return sfd;
}

/*
	buffer + *total is the point at which I want to start adding to the buffer
	(after the total amount of characters added before)

	*buffer_size - *total is the total amount of bytes allowed to be recv'd on
	this call

	no flags

	Returns:  0 = data received
			  1 = would block (EAGAIN/EWOULDBLOCK), caller should retry
			  2 = peer closed
			 -1 = connection closed or error
*/
int recv_chunks(
	int *client_fd,
	char *buffer,
	size_t *total,
	size_t *buffer_size
) {
	ssize_t recv_count;

	recv_count = recv(*client_fd, buffer + *total, *buffer_size - *total, 0);
	
	if (recv_count == 0) {
		return 2;
	}

	if (recv_count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 1;
		}
		
		return -1;
    }
	
	*total += recv_count;
	return 0;
}

int send_wrapper(
	int *client_fd,
	char *buffer,
	int buf_size
) {
	if (send(*client_fd, buffer, buf_size, MSG_NOSIGNAL) < 0) {
		if (errno == EPIPE || errno == ENOTCONN) {
			return -2;
		}

		return -1;
	}

	return 0;
}

int setnonblocking(
	int fd
) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}