#include "kvstore.h"
#include "kvs_protocol.h"
#include "server.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define KVS_MAX_TOKENS 8

// 根据命令类型获取最小参数个数（含命令关键字）
static int kvs_required_tokens(int cmd){
    switch(cmd){
        case KVS_CMD_SET:
        case KVS_CMD_MOD:
        case KVS_CMD_RSET:
        case KVS_CMD_RMOD:
        case KVS_CMD_HSET:
        case KVS_CMD_HMOD:
            return 3;
        default:
            return 2;
    }
}

// 统一追加 CRLF，保持协议响应格式
static int kvs_append_crlf(char *response){
    size_t len = strlen(response);
    if(len + 2 >= BUF_LEN){
        return KVS_ERR_INTERNAL;
    }
    response[len] = '\r';
    response[len + 1] = '\n';
    response[len + 2] = '\0';
    return (int)(len + 2);
}

// KV存储消息处理函数
int kvs_handler(char *msg, int length, char *response){
    if(msg == NULL || response == NULL || length <= 0){
        snprintf(response, BUF_LEN, "%s", kvs_strerror(KVS_ERR_PARAM));
        return kvs_append_crlf(response);
    }

    // 复制请求数据到本地缓冲区，避免原始数据被 tokenizer 修改
    int copy_len = length;
    if(copy_len >= BUF_LEN){
        copy_len = BUF_LEN - 1;
    }

    char buffer[BUF_LEN];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, msg, copy_len);
    buffer[copy_len] = '\0';

    // 解析 token
    char *tokens[KVS_MAX_TOKENS] = {0};
    int token_count = kvs_tokenizer(buffer, tokens);
    if(token_count <= 0){
        snprintf(response, BUF_LEN, "%s", kvs_strerror(KVS_ERR_PARAM));
        return kvs_append_crlf(response);
    }

    // 识别命令并校验参数数量
    int cmd = kvs_parser_command(tokens);
    if(cmd < KVS_CMD_START || cmd >= KVS_CMD_COUNT){
        snprintf(response, BUF_LEN, "ERROR Unknown command");
        return kvs_append_crlf(response);
    }

    if(token_count < kvs_required_tokens(cmd)){
        snprintf(response, BUF_LEN, "ERROR Missing arguments");
        return kvs_append_crlf(response);
    }

    memset(response, 0, BUF_LEN);
    // 执行命令填充响应
    int ret = kvs_executor_command(cmd, tokens, response);
    if(ret != KVS_OK){
        // kvs_executor_command已填充response，此处仅确保带有CRLF
        return kvs_append_crlf(response);
    }

    return kvs_append_crlf(response);
}

// 初始化KV存储
int kvs_init(){
    // 初始化数组
#if KVS_IS_ARRAY
    global_array = (kvs_array_t*)malloc(sizeof(kvs_array_t));
    if(global_array == NULL){
        return KVS_ERR_NOMEM;
        }
        kvs_array_create(global_array);
#endif

    // 初始化红黑树
#if KVS_IS_RBTREE
    int ret = kvs_rbtree_create(global_rbtree);
    if(ret != KVS_OK){
        return ret;
    }
#endif

    // 初始化哈希表
#if KVS_IS_HASH
    ret = kvs_hash_create(global_hash);
    if(ret != KVS_OK){
        return ret;
    }
#endif

    return KVS_OK;
}

int kvs_handle(struct conn* c){
    c->wbuff_len = kvs_handler(c->rbuff, c->rbuff_len, c->wbuff);
    return c->wbuff_len;
}

int kvs_encode(struct conn* c){
    return c->wbuff_len;
}


int main(int argc, char* argv[]){
    int port = 2000;
    // 没传端口号，就用默认的2000
    if(argc > 1){
        port = atoi(argv[1]);
    }

    log_server("Starting kvstore server on port %d...", port);
    
    // 初始化KV存储
    // kvs_init();

    // TODO 移除C不兼容的strncpy_s

    return reactor_mainloop(port, 1, kvs_handler);
}

// TODO 实现了Hash，要基于hash实现kvstore