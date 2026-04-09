# HTTP/1.1 Server in C

## What exists?
- TCP socket listening on port passed as argv[1]
- **Chunked Transfer Encoding**: Send any file chunk by chunk with hex chunk start indicator and chunk end terminator
- **Header Parsing**: Client recv buffer is read into memory (once, not in loop yet) and buffer is parsed looking for \r\n characters to create a pointer to the start of the string in memory and a count of the length of the string. No actual strings are created from the buffer, but instead we count on the buffer.
- **Routing**: All routes currently return `public/index.html`
- **JSON Responses**: HTTP responses with `Content-Type: application/json` and a status code can be sent via `send_json_response()`
- **HTTP Status Codes**: `http_status_str()` maps ~20 status codes to their reason phrase strings
- **`xmalloc`**: A malloc wrapper that prints on failure and returns NULL

## LIMArray

The LIMArray is an implementation that stores a pointer to an array of LineInMemory's, a count of the amount of items inside the array, and the total capacity of the array.

Each LineInMemory stores a pointer to the beginning of the array and a count of the length that should be read after the pointer to find the whole string (count = the length of the string). As mentioned before this is so that the strings do not have to be read into memory individually and rather the buffer can just be managed per recv cycle.

## StringView

StringView stores a pointer into a string and a character count, similar to LineInMemory but used for general string manipulation rather than header storage. The following operations are available:

- `remove_from_left(sv, amount)` — advance the pointer forward by `amount`
- `remove_from_right(sv, amount)` — shrink the count by `amount`
- `delim_from_left(sv, delim)` — strip leading occurrences of a delimiter character
- `delim_from_right(sv, delim)` — strip trailing occurrences of a delimiter character
- `trim_by_delim(sv, delim)` — strip both ends
