#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    int              sfd, s, total = 0;
    char             buf[BUF_SIZE] = {0};
    char             *buf_pointer = buf;
    ssize_t          nread;
    struct addrinfo  hints;
    struct addrinfo  *result, *rp;
    /* if (argc < 3) {
        fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
        exit(EXIT_FAILURE);
    } */

    /* Obtain address(es) matching host/port.  */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    
    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    
    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address.  */
    
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) break;
        close(sfd);
    }
    
    freeaddrinfo(result);
    
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    
    /* Send remaining command-line arguments as separate
       datagrams, and read responses from server.  */

    char *requested_file = "/index.html";
    char *j = 
        "GET /index.html HTTP/1.1\r\n"
        "Accept: application/javascript\r\n\r\n";

    int size = strlen(j) + 1;

    // send request
    if (write(sfd, j, size) != size) {
        fprintf(stderr, "partial/failed write\n");
        exit(EXIT_FAILURE);
    }

    // recieve the response
    for (;;) {

        nread = read(sfd, buf + total, BUF_SIZE - total - 1);
        if (nread == 0) break;
        if (nread == -1) {
            printf("Failed to read data\n");
            exit(EXIT_FAILURE);
        }

        total += nread;
        if (total >= BUF_SIZE) {
            exit(EXIT_FAILURE);
        }
        
        // skip headers
        buf_pointer = memmem(buf, total, "0\r\n\r\n", 5);
        if (buf_pointer != NULL) {
            buf_pointer += 5; // buf pointer should be at start of body;
            break;
        }
    }

//    printf("Received %zd bytes: %s\n", nread, buf);
//    printf("Total bytes received %d\n", total);

    // chunk header does include \r\n, while body only has automatic \n on end of lines!
    /*
    for (int i = 0; i < strlen(body); i++) {
        if (body[i] == 0x0D) {
            printf("/r\n");
            continue;
        }

        if (body[i] == 0x0A) {
            printf("/n\n");
            continue;
        }
        
        printf("%c", body[i]);
    }
    */

    printf("__FILE__: %s\n", __FILE__);
    FILE *fp = fopen(requested_file, "w");

    if (fp == NULL) {
        printf("Failed to open file.");
        exit(EXIT_FAILURE);
    }

    fwrite(buf, 1, sizeof(buf), fp);
    fclose(fp);

    for (;;) {
        buf_pointer = memmem(&buf_pointer, total, "\r\n", 2);
        // body might be null
        buf_pointer += 2; // move past the \r\n

        int distance = buf_pointer - buf + 1;
        char this_buf[distance];
        int snpr = snprintf(
            this_buf,
            distance,
            "%.*s\n",
            distance,
            buf_pointer
        );

        size_t fwr = fwrite(this_buf, 1, distance, fp);
        if (fwr == -1) {
            printf("Failed to write to file.");
            exit(EXIT_FAILURE);
        }

        printf("%s\n", buf_pointer);

        break;
    }

    fclose(fp);

    exit(EXIT_SUCCESS);
}
