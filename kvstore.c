#include "kvstore.h"

// 释放内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void kvs_free(void* ptr){
    return free(ptr);
}

// 分配内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void kvs_malloc(void* ptr){
    return malloc(ptr);
}

// 初始化KV存储
int kvs_init(){

}

int main(int argc, char* argv[]){
    int port = 2000;
    // 没传端口号，就用默认的2000
    if(argc > 1){
        port = atoi(argv[1]);
    }

    // TODO 移除C不兼容的strncpy_s
}

// TODO 实现了Hash，要基于hash实现kvstore