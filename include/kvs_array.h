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


// --- 函数原型声明 ---

/**
 * @brief 创建并初始化一个KV数组实例
 * @param ins 指向 kvs_array_t 结构体的指针
 * @return 成功返回0, 失败返回负数
 */
int kvs_array_create(kvs_array_t* ins);

/**
 * @brief 销毁一个KV数组实例，释放其占用的所有内存
 * @param ins 指向 kvs_array_t 结构体的指针
 */
void kvs_array_destroy(kvs_array_t* ins);

/**
 * @brief 根据key获取对应的value
 * @param ins 指向 kvs_array_t 结构体的指针
 * @param key 要查找的键
 * @return 找到则返回指向value的指针, 找不到返回NULL
 */
char* kvs_array_get(kvs_array_t* ins, char* key);

/**
 * @brief 向KV数组中设置一个新的键值对
 * @note 这个函数不允许更新已存在的key。
 * @param ins 指向 kvs_array_t 结构体的指针
 * @param key 要设置的键
 * @param val 要设置的值
 * @return 成功返回0, 失败返回负数
 */
int kvs_array_set(kvs_array_t* ins,  char* key, char* val);

/**
 * @brief 根据key删除一个键值对
 * @param ins 指向 kvs_array_t 结构体的指针
 * @param key 要删除的键
 * @return 成功返回0, 失败返回负数或未找到
 */
int kvs_array_del(kvs_array_t* ins, char* key);

/**
 * @brief 修改一个已存在的key所对应的value
 * @param ins 指向 kvs_array_t 结构体的指针
 * @param key 要修改的键
 * @param val 新的值
 * @return 成功返回0, 失败返回负数或未找到
 */
int kvs_array_mod(kvs_array_t* ins, char* key, char* val);

/**
 * @brief 检查一个key是否存在
 * @param ins 指向 kvs_array_t 结构体的指针
 * @param key 要检查的键
 * @return 存在返回0, 不存在返回1, 异常返回负数
 */
int kvs_array_exist(kvs_array_t* ins, char* key);


#endif // KVS_ARRAY_H