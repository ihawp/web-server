# HTTP/1.1 Server in C

## Useful Resources

- [HTTP/1.1](https://datatracker.ietf.org/doc/rfc9112/)
- [HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [HTTP Caching](https://www.rfc-editor.org/rfc/rfc9111.html)
- [Security Considerations](https://datatracker.ietf.org/doc/rfc9931/)
- [getaddrinfo (w/UDP example)](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
- [epoll (w/usage example)](https://www.man7.org/linux/man-pages/man7/epoll.7.html)
- [pthread](https://man7.org/linux/man-pages/man7/pthreads.7.html)
- [Threads](https://cs341.cs.illinois.edu/coursebook/Threads)
- Hash Table: [Repository](https://github.com/benhoyt/ht) & [Article](https://benhoyt.com/writings/hash-table-in-c/)

## About

This implementation uses *[sys/epoll.h](https://www.man7.org/linux/man-pages/man7/epoll.7.html)* and *[pthread.h](https://man7.org/linux/man-pages/man7/pthreads.7.html)* to create worker threads that monitor multiple client file descriptors for 'readiness' using **[epoll_wait(...)](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)**.

### Workers

When a new client connects the connection is accepted with **[accept(...)](https://man7.org/linux/man-pages/man2/accept.2.html)**. If the **client_fd** is successfully created it is added to the epoll monitoring list, where it will eventually be marked as ready. When a client file descriptor is marked as ready it is passed to **handle_request(...)** where it is treated as an HTTP/1.1 request. After the request is handled the file descriptor is removed from the epoll monitoring list.

### HTTP Lifecycle

When a request is made to the server and the client file descriptor has eventually been marked as ready, the HTTP lifecycle begins.

Since headers are expected at the beginning of any content sent by the client, we start by receiving chunks with **recv_header_chunks(...)** until we find the start of body indicator (**\r\n\r\n**), or something goes wrong (No data available, client closed connection, etc).

Once we find the start of body indicator we can safely assume two things. One, we have received all of the available headers for the request, and two, we may have received the beginning of the body already. The latter only matters if the user is attempting to send a body as part of their request (i.e not a GET, DELETE ... request).

When the latter is the case, we use the difference between the pointer at the beginning of the header string and the pointer to the **\r\n\r\n** sequence to determine how much of the body has already been read. Once determined, the pointer pointing at the **\r\n\r\n** sequence is incremented by 4 to move past the sequence to the beginning of the body text. We then use **recv_body_chunks(...)** to receive the rest of the bytes until the **http_request->content_length** is reached or exceeded.

### Headers

Header bytes are read into memory as described above, but they are parsed differently to the body, instead of copying all headers as individual readable strings, we instead store pointers to the beginning of each header line within the originally allocated header string and process them when we want to check the value.

Some values are stored after being read, such as the content-length which is read into memory as a long using **[strtol(...)](https://man7.org/linux/man-pages/man3/strtol.3.html)**. This value is stored in the **HTTPRequest** struct. Generally, values will not be stored, only the 'answer' to the value may be stored in the **HTTPResponse** struct if it is important.

It is safe to assume that the **HTTPRequest** struct will store data from the users request, while the **HTTPResponse** struct will store hints about how to respond to the users request, such as the status code.

### GET

GET requests attempt to dangerously find the file (risk of path traversal) in the */public* folder. If the file is found it is streamed to the client using **send_stream_file(...)**, which sends the file in chunks using Chunked Transfer-Encoding.

### StringView

Adapted from Tsoding's video on why C strings are terrible. Watch that video [here](https://www.youtube.com/watch?v=y8PLpDgZc0E).