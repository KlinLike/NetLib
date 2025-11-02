#ifndef __KV_STORE_H__
#define __KV_STORE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// 功能启用 ----------
#define KVS_IS_ARRAY    1   // 数组
#define KVS_IS_RBTREE   1   // 红黑树
#define KVS_IS_HASH     1   // 哈希表

// 配置参数 ----------
#define KVS_ARRAY_SIZE  100  // 数组默认大小

// 错误码定义 ----------
#define KVS_OK              0   // 成功
#define KVS_ERR_PARAM      -1   // 参数错误
#define KVS_ERR_NOMEM      -2   // 内存不足
#define KVS_ERR_NOTFOUND   -3   // 键不存在
#define KVS_ERR_EXISTS     -4   // 键已存在
#define KVS_ERR_INTERNAL   -5   // 内部错误

// 数据结构定义 ----------
typedef struct kvs_array_item_s {
    char *key;
    char *val;
} kvs_array_item_t;

typedef struct kvs_array_s {
    kvs_array_item_t *table;
    int count;
} kvs_array_t;

// 内存管理函数声明 ----------
void kvs_free(void* ptr);
void* kvs_malloc(size_t size);

// 函数声明 数组实现----------
int kvs_array_create(kvs_array_t* ins);
int kvs_array_destroy(kvs_array_t* ins);
int kvs_array_get(kvs_array_t* ins, char* key, char** value);
int kvs_array_set(kvs_array_t* ins, char* key, char* val);
int kvs_array_mod(kvs_array_t* ins, char* key, char* val);
int kvs_array_del(kvs_array_t* ins, char* key);
int kvs_array_exist(kvs_array_t* ins, char* key);

#endif