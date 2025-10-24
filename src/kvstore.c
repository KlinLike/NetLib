#include "kvstore.h"
#include "server.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>

// 释放内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void kvs_free(void* ptr){
    free(ptr);
}

// 分配内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void* kvs_malloc(size_t size){
    return malloc(size);
}

// KV存储消息处理函数
int kvs_handler(char *msg, int length, char *response){
    // TODO: 实现KV存储的具体业务逻辑
    // 目前为空，后续可以添加 SET/GET/DEL 等命令处理
    return 0;
}

// 初始化KV存储
int kvs_init(){
    // TODO: 初始化哈希表、红黑树等数据结构
    return 0;
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