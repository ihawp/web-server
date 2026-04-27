#include <stdio.h>
#include <stdlib.h>
#include "http_request.h"

void freeHTTPRequest(
	HTTPRequest *hrq
) {
	freelima(hrq->headers);
	memset(hrq->method, 0, REQ_METHOD_SIZE);
	memset(hrq->path, 0, REQ_PATH_SIZE);
	memset(hrq->http_version, 0, REQ_HTTP_VERSION_SIZE);
}