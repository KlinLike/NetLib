#include "server.h"
#include "logger.h"
#include <string.h>

// ECHO服务器的处理函数
// 负责将接收到的数据（rbuff）直接拷贝到发送缓冲区（wbuff）
int echo_handle(struct conn *c) {
    if (c == NULL) {
        log_error("echo_handle: connection is NULL");
        return -1;
    }

    if (c->rbuff_len <= 0) {
        log_warn("echo_handle: no data to echo (fd=%d)", c->fd);
        return 0;
    }

    if (c->rbuff_len > BUF_LEN) {
        log_error("echo_handle: data too large (fd=%d, len=%d)", c->fd, c->rbuff_len);
        return -1;
    }

    // 将接收到的数据原样拷贝到发送缓冲区
    memcpy(c->wbuff, c->rbuff, c->rbuff_len);
    c->wbuff_len = c->rbuff_len;

    log_debug("echo_handle: echoing %d bytes (fd=%d)", c->wbuff_len, c->fd);
    
    return c->wbuff_len;
}

// ECHO服务器的编码函数
// 对于简单的echo服务器，不需要特殊编码，数据已经在wbuff中准备好
int echo_encode(struct conn *c) {
    if (c == NULL) {
        log_error("echo_encode: connection is NULL");
        return -1;
    }

    // 对于echo服务器，数据已经在wbuff中，不需要额外编码
    // 这个函数主要用于一致性，如果将来需要添加协议头或编码逻辑可以在这里实现
    
    log_debug("echo_encode: ready to send %d bytes (fd=%d)", c->wbuff_len, c->fd);
    
    return c->wbuff_len;
}

