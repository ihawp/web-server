// this server expects to be sent a DGRAM message with a maximum byte length of 0
// from the linux man getaddrinfo(3) page
void UDPServer(char *port) {
	
	int sfd; // socket file descriptor
	int s; // addrinfo failure!?
	char buf[BUF_SIZE];
	ssize_t nread;
	socklen_t peer_addrlen;
	struct addrinfo h;
	struct addrinfo *result, *rp;
	struct sockaddr_storage peer_addr;

	// check argc (we already do in main())

	memset(&h, 0, sizeof(h));
	h.ai_flags = AI_PASSIVE | AI_NUMERICSERV; // int
	h.ai_family = AF_UNSPEC; // int
	h.ai_socktype = SOCK_DGRAM; // int
	h.ai_protocol = 0; // int
	h.ai_addr = NULL; // struct sockaddr
	h.ai_canonname = NULL; // char
	h.ai_next = NULL; // struct addrinfo

	// 0 is success
	s = getaddrinfo(NULL, port, &h, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully bind(2).
	   If socket(2) (or bind(2)) fails, we (close the socket and)
	   try the next address.   */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		
		if (sfd == -1) {
			continue;
		}

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}

		close(sfd);
	} 

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		char host[NI_MAXHOST], service[NI_MAXSERV];

		peer_addrlen = sizeof(peer_addr);
		nread = recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *) &peer_addr, &peer_addrlen);

		fadd2("request.log", buf);
		printf("%s", buf);

		if (nread == -1) {
			continue; // ignore failed request
		}

		s = getnameinfo((struct sockaddr *) &peer_addr, peer_addrlen, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);

		if (s == 0) {
			printf("Received %zd bytes from %s:%s\n", nread, host, service);
		}

		if (sendto(sfd, buf, nread, 0, (struct sockaddr *) &peer_addr, peer_addrlen) != nread) {
			fprintf(stderr, "Error sending response\n");
		}
	}
}
