#include "server.h"
#include <string.h>

static int first_line_is_http(const char* buf, int len){
    if(buf == NULL || len <= 0) return 0;
    int line_len = 0;
    for(int i = 0; i < len; ++i){
        if(buf[i] == '\n'){ line_len = i; break; }
    }
    if(line_len == 0){
        line_len = len;
        if(line_len > 512) line_len = 512;
    }
    char tmp[513];
    int copy = line_len;
    if(copy > 512) copy = 512;
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, buf, copy);
    return strstr(tmp, "HTTP/1.") != NULL;
}

static int is_http(const char* buf, int len){
    return first_line_is_http(buf, len);
}

int dispatcher_handler(struct conn* c){
    if(c == NULL) return -1;
    if(c->protocol == PROTO_UNKNOWN){
        if(is_http(c->rbuff, c->rbuff_len)){
            return http_handle(c);
        } else {
            return kvs_handle(c);
        }
    }

    if(c->protocol == PROTO_HTTP){
        return http_handle(c);
    } else {
        return kvs_handle(c);
    }
}
