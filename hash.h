#ifndef _HASH_H_
#define _HASH_H_

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


// --- 公共函数声明 (API) ---

/**
 * @brief 初始化一个哈希表。
 * @param hash 指向要初始化的哈希表结构体的指针。
 * @return 成功返回0，失败返回-1。
 */
int init_hashtable(hashtable_t *hash);

/**
 * @brief 销毁哈希表并释放所有相关资源。
 * @param hash 指向要销毁的哈希表的指针。
 */
void dest_hashtable(hashtable_t *hash);

/**
 * @brief 向哈希表中插入一个键值对。
 * @param hash 哈希表指针。
 * @param key 要插入的键。
 * @param value 要插入的值。
 * @return 成功返回0；如果键已存在，返回1；失败返回-1。
 */
int put_kv_hashtable(hashtable_t *hash, char *key, char *value);

/**
 * @brief 从哈希表中获取一个键对应的值。
 * @param hash 哈希表指针。
 * @param key 要查找的键。
 * @return 成功返回指向值的指针；如果键不存在，返回NULL。
 * @warning 返回的指针指向哈希表内部内存，请勿修改或释放它。
 */
char* get_kv_hashtable(hashtable_t *hash, char *key);

/**
 * @brief 从哈希表中删除一个键值对。
 * @param hash 哈希表指针。
 * @param key 要删除的键。
 * @return 成功返回0；如果键不存在或发生其他错误，返回负值。
 */
int delete_kv_hashtable(hashtable_t *hash, char *key);

/**
 * @brief 获取哈希表中键值对的总数。
 * @param hash 哈希表指针。
 * @return 当前存储的键值对数量。
 */
int count_kv_hashtable(hashtable_t *hash);

/**
 * @brief 检查一个键是否存在于哈希表中。
 * @param hash 哈希表指针。
 * @param key 要检查的键。
 * @return 如果存在，返回1；如果不存在，返回0。
 */
int exist_kv_hashtable(hashtable_t *hash, char *key);


#endif // _HASH_H_