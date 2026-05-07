#pragma once

#include <stdio.h>

int tcp_server(
	char *port
);

int recv_chunks(
	int *client_fd,
	char *buffer,
	size_t *total,
	size_t *buffer_size
);

int send_wrapper(
	int *client_fd,
	char *buffer,
	int buf_size
);

int setnonblocking(
	int fd
);