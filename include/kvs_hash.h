#ifndef _HASH_H_
#define _HASH_H_

#include "kvstore.h"
#include <pthread.h> // 因为 hashtable_t 结构体中包含了 pthread_mutex_t

// --- 公共宏定义 ---
// 用户会需要这些宏来定义自己的键和值缓冲区
#define MAX_KEY_LEN   128
#define MAX_VALUE_LEN 512

// --- 公共数据结构 ---

/**
 * @brief 哈希表的主结构体。
 * 使用者应该将此类型的变量传递给哈希表函数进行操作。
 * 注意：这是一个不透明的结构体，请不要直接访问其内部成员，
 * 除非你知道你在做什么。
 */
typedef struct hashtable_s {
    void **nodes;           // 使用 void** 来隐藏内部节点细节

    int max_slots;           // 哈希表的总槽位数（只读）
    int count;               // 当前存储的键值对数量（只读）

    pthread_mutex_t lock;    // 线程安全锁
    
} hashtable_t;

// --- 函数声明已移至 kvstore.h ---

#endif // _HASH_H_