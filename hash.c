#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

#define MAX_KEY_LEN 128
#define MAX_VAL_LEN 512
#define MAX_TABLE_SIZE  1024

typedef struct hashnode_s { // 哈希表节点
    char key[MAX_KEY_LEN];
    char val[MAX_VAL_LEN];

    struct hashnode_s* next;
} hashnode_t;


typedef struct hashtable_s { // 哈希表实例
    hashnode_t** nodes; // 拉链法：nodes 指向一个指针数组，这个数组的每一个元素，都是一个独立的、用于存放键值对的链表的头指针

    int max_slots;  // 哈希表内部数组的大小
    int count;      // 表当前存储的键值对数量

    pthread_mutex_t lock;
} hashtable_t;

hashtable_t hash;

// NOTE _ 开头的函数代表内部函数，不对外暴露

// 哈希函数，也就是计算 key -> int 的函数。但这是个很简单的实现，会有很高的冲突率。
static int _hash(char* key, int size){
    if(key == NULL) {
        return -1;
    }

    int sum = 0;
    int i = 0;
    while (key[i] != 0) {
        sum += key[i];
        ++i;
    }

    return sum % size;
}

// 创建节点
hashnode_t* _create_node(char* key, char* val){
    hashnode_t* node = (hashnode_t*)malloc(sizeof(hashnode_t));
    if(node == NULL) {
        return NULL;
    }
    
    strncpy(node->key, key, MAX_KEY_LEN - 1);
    node->key[MAX_KEY_LEN - 1] = '\0'; // 当源字符串长度等于或超过限制长度时，不会在目标数组末尾添加 \0
    strncpy(node->val, val, MAX_VAL_LEN - 1);
    node->val[MAX_VAL_LEN - 1] = '\0';
    node->next = NULL;

    return node;
}

// 初始化一个哈希表
int init_hashtable(hashtable_t* hash){
    if(hash == NULL) {
        return -1;
    }

    hash->nodes = (hashnode_t**)malloc(sizeof(hashnode_t*) * MAX_TABLE_SIZE); // 实际上是创建了一个指针数组
    if(hash->nodes == NULL) {
        return -2;
    }

    memset(hash->nodes, 0, sizeof(hashnode_t*) * MAX_TABLE_SIZE);
    hash->max_slots = MAX_TABLE_SIZE;
    hash->count = 0;

    pthread_mutex_init(&hash->lock, NULL);

    return 0;
}

// 销毁哈希表，释放所有相关资源,但还需要自己释放哈希表本身
void dest_hashtable(hashtable_t* hash) {
    if (hash == NULL) {
        return;
    }

    // 遍历所有桶和桶中的链表,释放每个节点
    for (int i = 0; i < hash->max_slots; ++i) {
        hashnode_t* node = hash->nodes[i];

        while (node != NULL) {
            hashnode_t* temp = node;
            node = node->next;
            free(temp);
        }
    }

    free(hash->nodes);
    hash->nodes = NULL;
    pthread_mutex_destroy(&hash->lock);
}

// 插入键值对，成功返回0，已存在返回1，失败返回-1
int put_hashtable(hashtable_t* hash, char* key, char* val){
    if(hash == NULL || key == NULL || val == NULL) {
        return -1;
    }

    int idx = _hash(key, MAX_TABLE_SIZE);

    pthread_mutex_lock(&hash->lock);

    // 遍历链表
    hashnode_t* node = hash->nodes[idx];
    while (node != NULL){
        if(strcmp(node->key, key) == 0){
            pthread_mutex_unlock(&hash->lock);
            return 1; // 已存在
        }
        node = node->next;
    }

    hashnode_t* new_node = _create_node(key, val);
    // 头插法
    new_node->next = hash->nodes[idx];
    hash->nodes[idx] = new_node;
    hash->count++;
    pthread_mutex_unlock(&hash->lock);

    return 0;
}

// 获取键值对，成功返回值，失败返回NULL
char* get_hashtable(hashtable_t* hash, char* key){
    if(hash == NULL || key == NULL) {
        return NULL;
    }

    int idx = _hash(key, MAX_TABLE_SIZE);

    pthread_mutex_lock(&hash->lock);

    hashnode_t* node = hash->nodes[idx];
    while(node != NULL){
        if(strcmp(node->key, key) == 0){
            pthread_mutex_unlock(&hash->lock);
            return node->val;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&hash->lock);
    return NULL;
}

// 计算哈希表中键值对的数量
int count_hashtable(hashtable_t* hash){
    return hash->count;
}

// 删除键值对，成功返回0，失败返回负值
int del_hashtable(hashtable_t* hash, char* key){
    if(hash == NULL || key == NULL) {
        return -1;
    }

    int idx = _hash(key, MAX_TABLE_SIZE);

    pthread_mutex_lock(&hash->lock);

    // 删除头结点需要特殊处理
    hashnode_t* head = hash->nodes[idx];
    if(head == NULL){
        return -2;
    }
    if(strcmp(head->key, key) == 0){
        hashnode_t* tmp = head->next;
        hash->nodes[idx] = tmp;
        free(head);

        hash->count--;
        pthread_mutex_unlock(&hash->lock);
        return 0;
    }

    // 删除非头结点
    hashnode_t* cur = head;
    // 1. 先搜索
    while(cur->next != NULL){
        if(strcmp(cur->next->key, key) == 0){
            break; // 找到了
        }
        cur = cur->next;
    }
    if(cur->next == NULL){
        pthread_mutex_unlock(&hash->lock);
        return -3; // 没找到
    }
    // 2. 删除
    hashnode_t* tmp = cur->next;
    cur->next = tmp->next;
    free(tmp);
    hash->count--;

    pthread_mutex_unlock(&hash->lock);
    return 0;
}

// 判断键是否存在，存在返回1，不存在返回0
int exist_hashtable(hashtable_t* hash, char* key){
    char* val = get_hashtable(hash, key);
    if(val == NULL) {
        return 0;
    }
    return 1;
}