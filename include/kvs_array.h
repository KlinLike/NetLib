#ifndef KVS_ARRAY_H
#define KVS_ARRAY_H

#include <stdlib.h> // 提供 malloc, free
#include <string.h> // 提供 strcmp, strlen, memset, strncpy_s

// --- 常量定义 ---
#define KVS_ARRAY_SIZE 1024 // 定义KV数组的默认大小

// --- 数据结构定义 ---

typedef struct kvs_array_item_s { // KV数组项
    char *key;
    char *val;
} kvs_array_item_t;
// TIP:在cpp中，这种typedef写法不是必须的。但在C中，如果不这样写，定义结构体类型需要加struct前缀，例如：struct kvs_array_s *array;

typedef struct kvs_array_s { // KV数组
    kvs_array_item_t *table;
    int count;  // 已保存的数据的数量
} kvs_array_t;

// --- 函数声明已移至 kvstore.h ---

#endif // KVS_ARRAY_H