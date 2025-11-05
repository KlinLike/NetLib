#include "hash.h"

#include <string.h>

#define HASH_DEFAULT_SLOTS 1024

// ========== 全局变量 ==========
hashtable_t global_hash_instance;
hashtable_t* global_hash = &global_hash_instance;

/*
 * 哈希表节点：链式拉链中的一个元素，保存键和值的副本
 * NOTE: 仅在本文件内声明，保证 hash.h 暴露的 hashtable_t 保持不透明，便于后续替换实现
 */
typedef struct hashnode_s {
    char key[MAX_KEY_LEN];
    char val[MAX_VALUE_LEN];
    struct hashnode_s *next;
} hashnode_t;

/* ---------- 工具函数 ---------- */

static inline hashnode_t **_hash_nodes(hashtable_t *hash) {
    return (hashnode_t **)hash->nodes;
}

static int _hash_index(const char *key, int size) {
    if (key == NULL || size <= 0) {
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

static hashnode_t *_hash_create_node(const char *key, const char *val) {
    hashnode_t *node = (hashnode_t *)kvs_malloc(sizeof(hashnode_t));
    if (node == NULL) {
        return NULL;
    }

    strncpy(node->key, key, MAX_KEY_LEN - 1);
    node->key[MAX_KEY_LEN - 1] = '\0'; // 当源字符串长度等于或超过限制长度时，不会在目标数组末尾添加 \0
    strncpy(node->val, val, MAX_VALUE_LEN - 1);
    node->val[MAX_VALUE_LEN - 1] = '\0';
    node->next = NULL;

    return node;
}

static void _hash_destroy_nodes(hashnode_t **nodes, int slots) {
    if (nodes == NULL) {
        return;
    }

    // 先遍历每个桶，释放每个桶中的链表节点
    for (int i = 0; i < slots; ++i) {
        hashnode_t *node = nodes[i];
        while (node != NULL) {
            hashnode_t *tmp = node->next;
            kvs_free(node);
            node = tmp;
        }
    }
    // 再释放桶数组
    kvs_free(nodes);
}

static int _hash_validate_key_value(const char *key, const char *val) {
    if (key == NULL || val == NULL) {
        return KVS_ERR_PARAM;
    }
    if ((int)strlen(key) >= MAX_KEY_LEN || (int)strlen(val) >= MAX_VALUE_LEN) {
        return KVS_ERR_PARAM;
    }
    return KVS_OK;
}

/* ---------- KVStore 对外接口 ---------- */

// 初始化哈希表：分配桶数组并准备互斥锁
int kvs_hash_create(hashtable_t *hash) {
    if (hash == NULL) {
        return KVS_ERR_PARAM;
    }

    // 分配桶数组
    hashnode_t **nodes = (hashnode_t **)kvs_malloc(sizeof(hashnode_t *) * HASH_DEFAULT_SLOTS);
    if (nodes == NULL) {
        return KVS_ERR_NOMEM;
    }

    memset(nodes, 0, sizeof(hashnode_t *) * HASH_DEFAULT_SLOTS);

    hash->nodes = (void **)nodes;
    hash->max_slots = HASH_DEFAULT_SLOTS;
    hash->count = 0;

    if (pthread_mutex_init(&hash->lock, NULL) != 0) {
        _hash_destroy_nodes(nodes, HASH_DEFAULT_SLOTS);
        hash->nodes = NULL;
        hash->max_slots = 0;
        return KVS_ERR_INTERNAL;
    }

    return KVS_OK;
}

// 销毁哈希表：释放节点和桶数组，销毁互斥锁
int kvs_hash_destroy(hashtable_t *hash) {
    if (hash == NULL) {
        return KVS_ERR_PARAM;
    }

    hashnode_t **nodes = _hash_nodes(hash);
    _hash_destroy_nodes(nodes, hash->max_slots);

    hash->nodes = NULL;
    hash->max_slots = 0;
    hash->count = 0;

    pthread_mutex_destroy(&hash->lock);
    return KVS_OK;
}

// 插入键值对：key 不存在时头插入链表
int kvs_hash_set(hashtable_t *hash, char *key, char *value) {
    if (hash == NULL) {
        return KVS_ERR_PARAM;
    }

    int check = _hash_validate_key_value(key, value);
    if (check != KVS_OK) {
        return check;
    }

    if (hash->nodes == NULL || hash->max_slots <= 0) {
        return KVS_ERR_INTERNAL;
    }

    int idx = _hash_index(key, hash->max_slots);
    if (idx < 0) {
        return KVS_ERR_PARAM;
    }

    hashnode_t **nodes = _hash_nodes(hash);

    pthread_mutex_lock(&hash->lock);

    hashnode_t *node = nodes[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            pthread_mutex_unlock(&hash->lock);
            return KVS_ERR_EXISTS;
        }
        node = node->next;
    }

    hashnode_t *new_node = _hash_create_node(key, value);
    if (new_node == NULL) {
        pthread_mutex_unlock(&hash->lock);
        return KVS_ERR_NOMEM;
    }

    new_node->next = nodes[idx];
    nodes[idx] = new_node;
    hash->count++;

    pthread_mutex_unlock(&hash->lock);
    return KVS_OK;
}

// 查询键值：命中返回内部 value 指针
int kvs_hash_get(hashtable_t *hash, char *key, char **value) {
    if (hash == NULL || value == NULL) {
        return KVS_ERR_PARAM;
    }
    *value = NULL;

    if (key == NULL || hash->nodes == NULL || hash->max_slots <= 0) {
        return KVS_ERR_PARAM;
    }

    int idx = _hash_index(key, hash->max_slots);
    if (idx < 0) {
        return KVS_ERR_PARAM;
    }

    hashnode_t **nodes = _hash_nodes(hash);

    pthread_mutex_lock(&hash->lock);

    hashnode_t *node = nodes[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            *value = node->val;
            pthread_mutex_unlock(&hash->lock);
            return KVS_OK;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&hash->lock);
    return KVS_ERR_NOTFOUND;
}

// 修改键值：仅在 key 存在时覆盖旧值
int kvs_hash_mod(hashtable_t *hash, char *key, char *value) {
    int check = _hash_validate_key_value(key, value);
    if (check != KVS_OK) {
        return check;
    }

    if (hash == NULL || hash->nodes == NULL || hash->max_slots <= 0) {
        return KVS_ERR_PARAM;
    }

    int idx = _hash_index(key, hash->max_slots);
    if (idx < 0) {
        return KVS_ERR_PARAM;
    }

    hashnode_t **nodes = _hash_nodes(hash);

    pthread_mutex_lock(&hash->lock);

    hashnode_t *node = nodes[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            strncpy(node->val, value, MAX_VALUE_LEN - 1);
            node->val[MAX_VALUE_LEN - 1] = '\0';
            pthread_mutex_unlock(&hash->lock);
            return KVS_OK;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&hash->lock);
    return KVS_ERR_NOTFOUND;
}

// 删除键值对：链表中定位并移除节点
int kvs_hash_del(hashtable_t *hash, char *key) {
    if (hash == NULL || key == NULL) {
        return KVS_ERR_PARAM;
    }

    if (hash->nodes == NULL || hash->max_slots <= 0) {
        return KVS_ERR_INTERNAL;
    }

    int idx = _hash_index(key, hash->max_slots);
    if (idx < 0) {
        return KVS_ERR_PARAM;
    }

    hashnode_t **nodes = _hash_nodes(hash);

    pthread_mutex_lock(&hash->lock);

    hashnode_t *prev = NULL;
    hashnode_t *node = nodes[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            if (prev == NULL) {
                nodes[idx] = node->next;
            } else {
                prev->next = node->next;
            }
            kvs_free(node);
            hash->count--;
            pthread_mutex_unlock(&hash->lock);
            return KVS_OK;
        }
        prev = node;
        node = node->next;
    }

    pthread_mutex_unlock(&hash->lock);
    return KVS_ERR_NOTFOUND;
}

// 判断键是否存在：复用 get 接口
int kvs_hash_exist(hashtable_t *hash, char *key) {
    char *value = NULL;
    int ret = kvs_hash_get(hash, key, &value);
    if (ret == KVS_OK) {
        return KVS_OK;
    }
    return ret;
}