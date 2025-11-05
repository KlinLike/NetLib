#ifndef __KV_STORE_H__
#define __KV_STORE_H__

/*
 * KVStore - 键值存储系统核心头文件
 * 
 * 本文件包含：
 *   - 错误码定义
 *   - 数据结构定义 (kvs_array_t)
 *   - 基础工具函数声明 (定义在 kvs_base.c)
 *   - 数据结构操作函数声明 (定义在 kvs_array.c)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ========== 功能启用配置 ==========
#define KVS_IS_ARRAY    1   // 数组
#define KVS_IS_RBTREE   1   // 红黑树
#define KVS_IS_HASH     1   // 哈希表

// ========== 配置参数 ==========
#define KVS_ARRAY_SIZE  1024    // 数组默认大小

// ========== 错误码定义 ==========
#define KVS_OK              0   // 成功
#define KVS_ERR_PARAM      -1   // 参数错误
#define KVS_ERR_NOMEM      -2   // 内存不足
#define KVS_ERR_NOTFOUND   -3   // 键不存在
#define KVS_ERR_EXISTS     -4   // 键已存在
#define KVS_ERR_INTERNAL   -5   // 内部错误 兜底错误

// ========== 数据结构定义 ==========
typedef struct kvs_array_item_s {
    char *key;
    char *val;
} kvs_array_item_t;

typedef struct kvs_array_s {
    kvs_array_item_t *table;
    int count;
} kvs_array_t;

// ========== 基础工具函数 (定义在 kvs_base.c) ==========

// 全局变量声明
extern kvs_array_t* global_array;

// 内存管理函数
void kvs_free(void* ptr);
void* kvs_malloc(size_t size);

// 错误处理函数
const char *kvs_strerror(int errnum);

// ========== 数组KVS操作函数 (定义在 kvs_array.c) ==========
int kvs_array_create(kvs_array_t* ins);
int kvs_array_destroy(kvs_array_t* ins);
int kvs_array_set(kvs_array_t* ins, char* key, char* val);
int kvs_array_get(kvs_array_t* ins, char* key, char** value);
int kvs_array_mod(kvs_array_t* ins, char* key, char* val);
int kvs_array_del(kvs_array_t* ins, char* key);
int kvs_array_exist(kvs_array_t* ins, char* key);

// ========== 红黑树相关类型和函数声明 (定义在 kvs_rbtree.c) ==========
#if KVS_IS_RBTREE

// 前向声明
typedef struct rbtree_s kvs_rbtree_t;

// 全局变量声明
extern kvs_rbtree_t* global_rbtree;

// 红黑树 KVS 操作函数
int kvs_rbtree_create(kvs_rbtree_t *inst);
int kvs_rbtree_destroy(kvs_rbtree_t *inst);
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_get(kvs_rbtree_t *inst, char *key, char **value);
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key);

#endif // KVS_IS_RBTREE

// ========== 哈希表相关类型和函数声明 (定义在 hash.c) ==========
#if KVS_IS_HASH

// 前向声明
typedef struct hashtable_s hashtable_t;

// 全局变量声明
extern hashtable_t* global_hash;

// 哈希表 KVS 操作函数（在 hash.h 中声明）
int kvs_hash_create(hashtable_t *hash);
int kvs_hash_destroy(hashtable_t *hash);
int kvs_hash_set(hashtable_t *hash, char *key, char *value);
int kvs_hash_get(hashtable_t *hash, char *key, char **value);
int kvs_hash_mod(hashtable_t *hash, char *key, char *value);
int kvs_hash_del(hashtable_t *hash, char *key);
int kvs_hash_exist(hashtable_t *hash, char *key);

#endif // KVS_IS_HASH

#endif