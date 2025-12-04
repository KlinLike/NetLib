#include "server.h"
#include <string.h>

static const char* HELLO_BODY = "<html><body>Hello</body></html>";

int http_handle(struct conn* c){
    const char* body = HELLO_BODY;
    // 需要先算body长度，才能计算header
    int body_len = (int)strlen(body);

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=utf-8\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     body_len);

    memset(c->wbuff, 0, BUF_LEN);
    c->wbuff_len = 0;
    c->wbuff_sent = 0;

    if(header_len + body_len >= BUF_LEN){
        return -1;
    }

    memcpy(c->wbuff + c->wbuff_len, header, header_len);
    c->wbuff_len += header_len;
    memcpy(c->wbuff + c->wbuff_len, body, body_len);
    c->wbuff_len += body_len;

    c->should_close = 1;
    if(c->protocol == PROTO_UNKNOWN){
        c->protocol = PROTO_HTTP;
    }
    return c->wbuff_len;
}
