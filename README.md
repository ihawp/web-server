# HTTP/1.1 Server in C

## TODO:
- Post requests broken by design.

## What exists?

- TCP socket listening on port passed as `argv[1]`
- **Chunked Transfer Encoding**: Send any file chunk by chunk with hex chunk size header and terminating `0\r\n\r\n`
- **Header Parsing**: Client recv buffer is read in a loop until the header terminator `\r\n\r\n` is found, then parsed by scanning for `\r\n` sequences to build an array of `LineInMemory` entries — each a pointer into the buffer and a byte count, so no individual string copies are made
- **Routing**: Requests to `/` serve `public/index.html`; all other paths are served from `public/<path>`
- **JSON Responses**: HTTP responses with `Content-Type: application/json` and a status code can be sent via `send_json_response()`
- **HTTP Status Codes**: `http_status_str()` maps ~20 status codes to their reason phrase strings
- **`xmalloc`**: A malloc wrapper that prints on failure and returns NULL
- **Request Timeout**: A 500ms receive timeout (`SO_RCVTIMEO`) is set on each accepted client socket

## LIMArray

The `LIMArray` is a dynamic array of `LineInMemory` structs. It stores a pointer to the backing array, a count of current items, and a total capacity — doubling in size via `realloc` when capacity is exceeded (via the `arr_append` macro).

Each `LineInMemory` stores a pointer to the start of a line within the receive buffer and a byte count for its length. No string copies are made; the buffer itself is the backing store.

## StringView

`StringView` stores a pointer into a string and a character count, similar to `LineInMemory` but used for general string manipulation rather than header storage. The following operations are available:
- `remove_from_left(sv, amount)` — advance the pointer forward by `amount`
- `remove_from_right(sv, amount)` — shrink the count by `amount`
- `delim_from_left(sv, delim)` — strip leading occurrences of a delimiter character
- `delim_from_right(sv, delim)` — strip trailing occurrences of a delimiter character
- `trim_by_delim(sv, delim)` — strip both ends
- `split_by_delim(sv, delim)` — split off and return the portion before the first occurrence of `delim`, advancing `sv` past it

## HTTPRequest

`HTTPRequest` holds the parsed first line of the request — `method`, `path`, and `http_version` — as fixed-size char arrays, plus a pointer to the full `LIMArray` of raw header lines. Parsing is done in `double_pass_headers()`, which also iterates remaining headers and matches against known header names via the `HDR` macro (handlers are currently stubs).