#include "kvstore.h"
#include "kvs_protocol.h"
#include "server.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>

// KV存储消息处理函数
int kvs_handler(char *msg, int length, char *response){
    // TODO: 实现KV存储的具体业务逻辑
    // 目前为空，后续可以添加 SET/GET/DEL 等命令处理
    return 0;
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
    // TODO: 发送响应数据到客户端
}


int main(int argc, char* argv[]){
    int port = 2000;
    // 没传端口号，就用默认的2000
    if(argc > 1){
        port = atoi(argv[1]);
    }

    log_server("Starting echo server on port %d...", port);
    
    // 初始化KV存储
    // kvs_init();

    // TODO 移除C不兼容的strncpy_s

    return reactor_mainloop(port, 1, kvs_handler);
}

// TODO 实现了Hash，要基于hash实现kvstore