#include "server.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

// ============== SHA1 实现 (用于 WebSocket 握手) ==============
// 简化版 SHA1，仅用于 WebSocket 握手

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e, w[80];
    int i;
    
    for (i = 0; i < 16; i++) {
        w[i] = (buffer[i*4] << 24) | (buffer[i*4+1] << 16) | 
               (buffer[i*4+2] << 8) | buffer[i*4+3];
    }
    for (i = 16; i < 80; i++) {
        uint32_t t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (t << 1) | (t >> 31);
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    
    for (i = 0; i < 80; i++) {
        uint32_t f, k, temp;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t hash[20]) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buffer[64];
    size_t i, j = 0;
    
    for (i = 0; i + 64 <= len; i += 64) {
        sha1_transform(state, data + i);
    }
    
    // 填充
    memset(buffer, 0, 64);
    j = len - i;
    memcpy(buffer, data + i, j);
    buffer[j] = 0x80;
    
    if (j >= 56) {
        sha1_transform(state, buffer);
        memset(buffer, 0, 64);
    }
    
    // 长度（位）
    uint64_t bits = len * 8;
    for (i = 0; i < 8; i++) {
        buffer[63 - i] = (uint8_t)(bits >> (i * 8));
    }
    sha1_transform(state, buffer);
    
    for (i = 0; i < 5; i++) {
        hash[i*4]   = (state[i] >> 24) & 0xFF;
        hash[i*4+1] = (state[i] >> 16) & 0xFF;
        hash[i*4+2] = (state[i] >> 8) & 0xFF;
        hash[i*4+3] = state[i] & 0xFF;
    }
}

// ============== Base64 编码 ==============

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *in, size_t len, char *out) {
    size_t i, j = 0;
    for (i = 0; i + 2 < len; i += 3) {
        out[j++] = base64_table[in[i] >> 2];
        out[j++] = base64_table[((in[i] & 0x03) << 4) | (in[i+1] >> 4)];
        out[j++] = base64_table[((in[i+1] & 0x0F) << 2) | (in[i+2] >> 6)];
        out[j++] = base64_table[in[i+2] & 0x3F];
    }
    if (i < len) {
        out[j++] = base64_table[in[i] >> 2];
        if (i + 1 < len) {
            out[j++] = base64_table[((in[i] & 0x03) << 4) | (in[i+1] >> 4)];
            out[j++] = base64_table[(in[i+1] & 0x0F) << 2];
        } else {
            out[j++] = base64_table[(in[i] & 0x03) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
    return (int)j;
}

// ============== WebSocket 握手 ==============

static const char *WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// 从 HTTP 头中提取 Sec-WebSocket-Key
static int get_ws_key(const char *buf, int len, char *key_out, int key_size) {
    const char *p = buf;
    const char *end = buf + len;
    const char *header = "Sec-WebSocket-Key:";
    int header_len = strlen(header);
    
    while (p < end) {
        if (strncasecmp(p, header, header_len) == 0) {
            p += header_len;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            int i = 0;
            while (p < end && *p != '\r' && *p != '\n' && i < key_size - 1) {
                key_out[i++] = *p++;
            }
            key_out[i] = '\0';
            return i;
        }
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
    return 0;
}

// 检查是否是 WebSocket 升级请求
static int is_ws_upgrade(const char *buf, int len) {
    if (len < 4) return 0;
    // 检查是否包含 Upgrade: websocket
    const char *p = buf;
    const char *end = buf + len;
    int has_upgrade = 0;
    int has_connection = 0;
    
    while (p < end) {
        if (strncasecmp(p, "Upgrade:", 8) == 0) {
            if (strstr(p, "websocket") || strstr(p, "WebSocket")) {
                has_upgrade = 1;
            }
        }
        if (strncasecmp(p, "Connection:", 11) == 0) {
            if (strstr(p, "Upgrade") || strstr(p, "upgrade")) {
                has_connection = 1;
            }
        }
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
    return has_upgrade && has_connection;
}

// 生成握手响应
static int ws_handshake_response(const char *client_key, char *response, int resp_size) {
    // 拼接 key + magic
    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_MAGIC);
    
    // SHA1
    uint8_t hash[20];
    sha1((uint8_t*)combined, strlen(combined), hash);
    
    // Base64
    char accept_key[64];
    base64_encode(hash, 20, accept_key);
    
    // 构建响应
    return snprintf(response, resp_size,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);
}

// ============== WebSocket 帧处理 ==============

// 解析 WebSocket 帧，返回 payload 起始位置和长度
// 返回值: 0=成功, -1=数据不完整, -2=错误
static int ws_parse_frame(const char *buf, int len, 
                          int *opcode, char **payload, int *payload_len) {
    if (len < 2) return -1;
    
    uint8_t *data = (uint8_t*)buf;
    // int fin = (data[0] >> 7) & 0x01;
    *opcode = data[0] & 0x0F;
    int mask = (data[1] >> 7) & 0x01;
    uint64_t plen = data[1] & 0x7F;
    
    int header_len = 2;
    
    if (plen == 126) {
        if (len < 4) return -1;
        plen = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (plen == 127) {
        if (len < 10) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++) {
            plen = (plen << 8) | data[2 + i];
        }
        header_len = 10;
    }
    
    if (mask) header_len += 4;
    
    if ((int)(header_len + plen) > len) return -1;
    
    *payload_len = (int)plen;
    *payload = (char*)(buf + header_len);
    
    // 解除掩码
    if (mask && plen > 0) {
        uint8_t *mask_key = data + header_len - 4;
        uint8_t *p = (uint8_t*)*payload;
        for (uint64_t i = 0; i < plen; i++) {
            p[i] ^= mask_key[i % 4];
        }
    }
    
    return 0;
}

// 构建 WebSocket 帧（服务端发送，无掩码）
static int ws_build_frame(int opcode, const char *payload, int payload_len, 
                          char *out, int out_size) {
    if (payload_len < 0) return -1;
    
    int header_len;
    uint8_t *data = (uint8_t*)out;
    
    data[0] = 0x80 | (opcode & 0x0F);  // FIN=1, opcode
    
    if (payload_len < 126) {
        data[1] = payload_len;
        header_len = 2;
    } else if (payload_len < 65536) {
        data[1] = 126;
        data[2] = (payload_len >> 8) & 0xFF;
        data[3] = payload_len & 0xFF;
        header_len = 4;
    } else {
        data[1] = 127;
        for (int i = 0; i < 8; i++) {
            data[2 + i] = (payload_len >> ((7 - i) * 8)) & 0xFF;
        }
        header_len = 10;
    }
    
    if (header_len + payload_len > out_size) return -1;
    
    memcpy(out + header_len, payload, payload_len);
    return header_len + payload_len;
}

// ============== WebSocket Handler ==============

int ws_handle(struct conn *c) {
    if (c == NULL) return -1;
    
    // 还没有被设为WS协议，说明是握手请求
    if (c->protocol != PROTO_WS) {
        log_info("[WS] fd=%d: Received upgrade request", c->fd);
        
        if (!is_ws_upgrade(c->rbuff, c->rbuff_len)) {
            log_info("[WS] fd=%d: Not a valid WebSocket upgrade request", c->fd);
            return -1;
        }
        
        // 提取 Sec-WebSocket-Key
        char ws_key[64] = {0};
        if (get_ws_key(c->rbuff, c->rbuff_len, ws_key, sizeof(ws_key)) == 0) {
            log_info("[WS] fd=%d: Missing Sec-WebSocket-Key", c->fd);
            return -1;
        }
        
        log_info("[WS] fd=%d: Handshake success, key=%s", c->fd, ws_key);
        
        // 生成握手响应
        c->wbuff_len = ws_handshake_response(ws_key, c->wbuff, BUF_LEN);
        c->wbuff_sent = 0;
        c->protocol = PROTO_WS;
        c->should_close = 0;  // WebSocket 是长连接
        
        return c->wbuff_len;
    }
    
    // 已经是 WebSocket 连接，解析帧
    int opcode;
    char *payload;
    int payload_len;
    
    int ret = ws_parse_frame(c->rbuff, c->rbuff_len, &opcode, &payload, &payload_len);
    if (ret < 0) {
        log_info("[WS] fd=%d: Frame parse failed (ret=%d)", c->fd, ret);
        return ret;
    }
    
    c->wbuff_len = 0;
    c->wbuff_sent = 0;
    
    switch (opcode) {
        case 0x1:  // 文本帧
            log_info("[WS] fd=%d: Text frame, len=%d, data=\"%.*s\"", 
                     c->fd, payload_len, payload_len > 64 ? 64 : payload_len, payload);
            c->wbuff_len = ws_build_frame(opcode, payload, payload_len, 
                                          c->wbuff, BUF_LEN);
            break;
            
        case 0x2:  // 二进制帧
            log_info("[WS] fd=%d: Binary frame, len=%d", c->fd, payload_len);
            c->wbuff_len = ws_build_frame(opcode, payload, payload_len, 
                                          c->wbuff, BUF_LEN);
            break;
            
        case 0x8:  // 关闭帧
            log_info("[WS] fd=%d: Close frame received", c->fd);
            c->wbuff_len = ws_build_frame(0x8, "", 0, c->wbuff, BUF_LEN);
            c->should_close = 1;
            break;
            
        case 0x9:  // Ping
            log_info("[WS] fd=%d: Ping received, sending Pong", c->fd);
            c->wbuff_len = ws_build_frame(0xA, payload, payload_len, 
                                          c->wbuff, BUF_LEN);
            break;
            
        case 0xA:  // Pong
            log_info("[WS] fd=%d: Pong received", c->fd);
            break;
            
        default:
            log_info("[WS] fd=%d: Unknown opcode=0x%x", c->fd, opcode);
            return -1;
    }
    
    return c->wbuff_len;
}
