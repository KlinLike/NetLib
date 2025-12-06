#include "server.h"
#include <string.h>

// 检查是否是 WebSocket 升级请求
static int is_ws_upgrade_request(const char* buf, int len) {
    if (len < 20) return 0;
    // 必须同时包含 Upgrade: websocket 和 Connection: Upgrade
    int has_upgrade = 0;
    int has_connection = 0;
    const char *p = buf;
    const char *end = buf + len;
    
    while (p < end) {
        if (strncasecmp(p, "Upgrade:", 8) == 0) {
            const char *line_end = p;
            while (line_end < end && *line_end != '\n') line_end++;
            // 在这一行中查找 websocket
            for (const char *q = p; q < line_end; q++) {
                if (strncasecmp(q, "websocket", 9) == 0) {
                    has_upgrade = 1;
                    break;
                }
            }
        }
        if (strncasecmp(p, "Connection:", 11) == 0) {
            const char *line_end = p;
            while (line_end < end && *line_end != '\n') line_end++;
            for (const char *q = p; q < line_end; q++) {
                if (strncasecmp(q, "Upgrade", 7) == 0) {
                    has_connection = 1;
                    break;
                }
            }
        }
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
    return has_upgrade && has_connection;
}

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

// 协议分发器
// 根据连接的协议类型或请求内容，分发到对应的处理函数
int dispatcher_handler(struct conn* c){
    if(c == NULL) return -1;
    
    // 已识别协议的连接，直接分发
    switch(c->protocol) {
        case PROTO_WS:   return ws_handle(c);
        case PROTO_HTTP: return http_handle(c);
        case PROTO_KVS:  return kvs_handle(c);
        default: break;
    }
    
    // 新连接，根据请求内容识别协议
    // 优先级：WebSocket升级 > HTTP > KVS
    if(is_http(c->rbuff, c->rbuff_len)){
        if(is_ws_upgrade_request(c->rbuff, c->rbuff_len)){
            return ws_handle(c);  // ws_handle 内部会设置 protocol = PROTO_WS
        }
        return http_handle(c);
    }
    
    // 非 HTTP 请求，当作 KVS 处理
    return kvs_handle(c);
}
