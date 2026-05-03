#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "tcp_server.h"
#include "http.h"


// create http_workers here.
int main(
	int argc,
	char **argv
) {
	char *port;
	int sfd;

	if (argc < 2) {
		printf(
			"Server startup failed, try:\n\n"
			"<Server Name> <PORT>\n\n"
			"Example:\n\n"
			"./server 2222\n"
		);
		exit(EXIT_FAILURE);
	}

	port = argv[1];
	sfd = tcp_server(port);

	if (sfd == -1) {
		exit(EXIT_FAILURE);
	}

	http_server(sfd, port);

	pause();
}