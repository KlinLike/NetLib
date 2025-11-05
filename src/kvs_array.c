#include "kvstore.h"
#include <stdlib.h>
#include <string.h>

// 5 + 2
// 2个创建删除数据结构的函数 create destroy
// 5个操作数据的函数 get set mod del exist

// 创建KV数组
int kvs_array_create(kvs_array_t* ins){
    if(ins == NULL){
        return KVS_ERR_PARAM;
    }
    if(ins->table != NULL){
        return KVS_ERR_INTERNAL;
    }
    
    ins->table = (kvs_array_item_t*)malloc(sizeof(kvs_array_item_t) * KVS_ARRAY_SIZE);
    if(ins->table == NULL){
        return KVS_ERR_NOMEM;
    }

    ins->count = 0;
    return KVS_OK;
}

// 销毁KV数组
int kvs_array_destroy(kvs_array_t* ins){
    if(ins == NULL){
        return KVS_ERR_PARAM;
    }

    if(ins->table != NULL){
        kvs_free(ins->table);
        ins->table = NULL;
        ins->count = 0;
    }
    return KVS_OK;
}

// 获取KV数组中的值，找不到会返回KVS_ERR_NOTFOUND
int kvs_array_get(kvs_array_t* ins, char* key, char** value){
    if(ins == NULL || key == NULL || value == NULL){
        return KVS_ERR_PARAM;
    }
    
    int i = 0;
    for (i = 0; i < ins->count; i++){
        if(ins->table[i].key != NULL && strcmp(ins->table[i].key, key) == 0){
            *value = ins->table[i].val;
            return KVS_OK;
        }
    }
    
    *value = NULL;
    return KVS_ERR_NOTFOUND;
}
    
// 设置KV数组中的值
int kvs_array_set(kvs_array_t* ins,  char* key, char* val){
    if(ins == NULL || key == NULL || val == NULL){
        return KVS_ERR_PARAM;
    }
    if(ins->count >= KVS_ARRAY_SIZE){
        // TODO: 扩容？
        // 用total作为已满的判断依据是合理的，因为total是保存的数据总数，而不是有意义数据的最大下标的值
        return KVS_ERR_NOMEM; // 已满
    }

    // 创建空间保存key和val
    char* copykey = (char*)malloc(sizeof(char) * (strlen(key) + 1));
    if(copykey == NULL){
        return KVS_ERR_NOMEM;
    }
    memset(copykey, 0, strlen(key) + 1);
    strcpy(copykey, key);

    char* copyval = (char*)malloc(sizeof(char) * (strlen(val) + 1));
    if(copyval == NULL){
        free(copykey);
        return KVS_ERR_NOMEM;
    }
    memset(copyval, 0, strlen(val) + 1);
    strcpy(copyval, val);

    // 一次遍历同时完成两个任务：1.检查key是否存在 2.找到可插入的位置
    int i = 0; // C99标准前不允许使用for循环的初始化语句
    int empty_pos = -1; // 记录第一个空位置
    for (i = 0; i < ins->count; i++){
        if(empty_pos == -1 && ins->table[i].key == NULL){
            // 记录第一个空位置
            empty_pos = i;
        }
        if(ins->table[i].key != NULL && strcmp(ins->table[i].key, key) == 0){
            // key已经存在，释放刚分配的内存并返回1
            free(copykey);
            free(copyval);
            return KVS_ERR_EXISTS;
        }
    }

    // key不存在，可以插入
    if(empty_pos != -1){
        // 中间有空位置，填充空洞
        ins->table[empty_pos].key = copykey;
        ins->table[empty_pos].val = copyval;
        // 不增加 count，因为这是填充空洞
        return KVS_OK;
    } else if(ins->count < KVS_ARRAY_SIZE){
        // 中间没有空位置，在末端插入
        ins->table[ins->count].key = copykey;
        ins->table[ins->count].val = copyval;
        ins->count++;
        return KVS_OK;
    }
    
    // 理论上不会到这里，因为开头已经检查过容量
    free(copykey);
    free(copyval);
    return KVS_ERR_INTERNAL;
}

// 删除KV数组中的值，异常返回负数，找不到返回KVS_ERR_NOTFOUND
int kvs_array_del(kvs_array_t* ins, char* key){
    if(ins == NULL || key == NULL){
        return KVS_ERR_PARAM;
    }

    if(ins->count == 0){
        return KVS_ERR_NOTFOUND;
    }

    for (int i = 0; i < ins->count; i++){
        if(ins->table[i].key == NULL){
            continue;
        }
        if(strcmp(ins->table[i].key, key) == 0){
            kvs_free(ins->table[i].key);
            ins->table[i].key = NULL;
            kvs_free(ins->table[i].val);
            ins->table[i].val = NULL;
            ins->count--;
            return KVS_OK;
        }
    }
    return KVS_ERR_NOTFOUND;
}
// NOTE: 删除后的空洞如何处理？
// 1. 用最后一个元素填补空洞 快速 改变顺序
// 2. 把后面的速度往前移动 慢 保持顺序

// 修改KV数组中的值，异常返回负数，找不到返回KVS_ERR_NOTFOUND
int kvs_array_mod(kvs_array_t* ins, char* key, char* val){
    if(ins == NULL || key == NULL || val == NULL){
        return KVS_ERR_PARAM;
    }
    if(ins->count == 0){
        return KVS_ERR_NOTFOUND;
    }

    for (int i = 0; i < ins->count; i++) {
        if (ins->table[i].key != NULL && strcmp(ins->table[i].key, key) == 0) {
            char* copyval = (char*)malloc(strlen(val) + 1);
            if (copyval == NULL) {
                return KVS_ERR_NOMEM;
            }
            strcpy(copyval, val);
            kvs_free(ins->table[i].val);
            ins->table[i].val = copyval;
            return KVS_OK;
        }
    }

    return KVS_ERR_NOTFOUND;
}

int kvs_array_exist(kvs_array_t* ins, char* key){
    if(ins == NULL || key == NULL){
        return KVS_ERR_PARAM;
    }

    char* val = NULL;
    int ret = kvs_array_get(ins, key, &val);
    if (ret == KVS_OK){
        return KVS_OK;  // 0 表示存在
    }
    return KVS_ERR_NOTFOUND;  // -3 表示不存在
}