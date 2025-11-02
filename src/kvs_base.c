#include "kvstore.h"
#include <stdlib.h>

// NOTE: 对应头文件kvs_store.h

// 定义全局变量
kvs_array_t* global_array = NULL;

// 释放内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void kvs_free(void* ptr){
    free(ptr);
}

// 分配内存 -> 封装的好处是，如果将来需要改变内存释放的方式，只需要修改这个函数
void* kvs_malloc(size_t size){
    return malloc(size);
}

// 错误码转字符串
const char *kvs_strerror(int errnum){
    switch(errnum){
        case KVS_OK:
            return "OK";
        case KVS_ERR_PARAM:
            return "ERROR: Invalid parameter";
        case KVS_ERR_NOMEM:
            return "ERROR: Out of memory";
        case KVS_ERR_NOTFOUND:
            return "ERROR: Key not found";
        case KVS_ERR_EXISTS:
            return "ERROR: Key already exists";
        case KVS_ERR_INTERNAL:
            return "ERROR: Internal error";
        default:
            return "ERROR: Unknown error";
    }
}

