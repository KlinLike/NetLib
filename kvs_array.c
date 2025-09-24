#include "kvstore.h"

// 5 + 2
// 2个创建删除数据结构的函数 create destroy
// 5个操作数据的函数 get set mod del exist

// 创建KV数组
int kvs_array_create(kvs_array_t* ins){
    // 参数检查
    if(ins == NULL){
        return -1;
    }
    if(ins->table != NULL){
        return -2;
    }
    
    ins->table = (kvs_array_item_t*)malloc(sizeof(kvs_array_item_t) * KVS_ARRAY_SIZE);
    if(ins->table == NULL){
        return -3;
    }

    ins->count = 0;
    return 0;
}

// 销毁KV数组
void kvs_array_destroy(kvs_array_t* ins){
    // 参数检查
    if(ins == NULL){
        return;
    }

    if(ins->table != NULL){
        kvs_free(ins->table);
    }
}

// 获取KV数组中的值，找不到会返回NULL
char* kvs_array_get(kvs_array_t* ins, char* key){
    // 参数检查
    if(ins == NULL || key == NULL){
        return NULL;
    }

    int i = 0;
    for (i = 0; i < ins->count; i++){
        if(ins->table[i].key != NULL && strcmp(ins->table[i].key, key) == 0){
            return ins->table[i].val;
        }
    }
    return NULL;
}
    
// 设置KV数组中的值，要注意的是这个函数不能更新值
int kvs_array_set(kvs_array_t* ins,  char* key, char* val){
    // 参数检查
    if(ins == NULL || key == NULL || val == NULL){
        return -1;
    }
    if(ins->count >= KVS_ARRAY_SIZE){
        // TODO: 扩容？
        // 用total作为已满的判断依据是合理的，因为total是保存的数据总数，而不是有意义数据的最大下标的值
        return -2; // 已满
    }

    // 创建空间保存key和val
    char* copykey = (char*)malloc(sizeof(char) * (strlen(key) + 1));
    if(copykey == NULL){
        return -3;
    }
    memset(copykey, 0, strlen(key) + 1);
    strncpy_s(copykey, strlen(key) + 1, key, strlen(key));

    char* copyval = (char*)malloc(sizeof(char) * (strlen(val) + 1));
    if(copyval == NULL){
        return -3;
    }
    memset(copyval, 0, strlen(val) + 1);
    strncpy_s(copyval, strlen(val) + 1, val, strlen(val));

    // 找找看中间有没有空的位置可以插入
    int i = 0; // C99标准前不允许使用for循环的初始化语句
    for (i = 0; i < ins->count; i++){
        if(ins->table[i].key == NULL){
            ins->table[i].key = copykey;
            ins->table[i].val = copyval;
            ins->count++;
            return 0;
        }
    }
    // 中间没有空的位置可以插入，需要在末端插入
    if(i == ins->count && i < KVS_ARRAY_SIZE){
        ins->table[i].key = copykey;
        ins->table[i].val = copyval;
        ins->count++;
    } // 这里其实不用判断已满，因为上面已经判断过了
    
    return 0;
}

// 删除KV数组中的值，异常返回负数，找不到返回搜索终止下标
int kvs_array_del(kvs_array_t* ins, char* key){
    // 参数检查
    if(ins == NULL || key == NULL){
        return -1;
    }

    if(ins->count == 0){
        return -1; // 这里必须检查，不然等下可能会返回 i = 0
    }

    int i = 0;
    while (i < ins->count){
        if(ins->table[i].key == NULL){
            continue;
        }
        if(strcmp(ins->table[i].key, key) == 0){
            kvs_free(ins->table[i].key);
            ins->table[i].key = NULL;
            kvs_free(ins->table[i].val);
            ins->table[i].val = NULL;
            ins->count--;
            return 0;
        }
        ++i;
    }
    return i;
}

// 修改KV数组中的值，异常返回负数，找不到返回搜索终止下标
int kvs_array_mod(kvs_array_t* ins, char* key, char* val){
    // 参数检查
    if(ins == NULL || key == NULL || val == NULL){
        return -1;
    }
    if(ins->count == 0){
        return -2;
    }

    int i = 0;
    while (i < ins->count){
        if(ins->table[i].key == NULL){
            continue;
        }
        if(strcmp(ins->table[i].key, key) == 0){
            kvs_free(ins->table[i].val);
            char* copyval = (char*)malloc(sizeof(char) * (strlen(val) + 1));
            if(copyval == NULL){
                return -3;
            }
            memset(copyval, 0, strlen(val) + 1);
            strncpy_s(copyval, strlen(val) + 1, val, strlen(val));
            ins->table[i].val = copyval;
            return 0;
        }
        ++i;
    }
    return i;
}

int kvs_array_exist(kvs_array_t* ins, char* key){
    // 参数检查
    if(ins == NULL || key == NULL){
        return -1;
    }

    char* val = kvs_array_get(ins, key);
    if (val == NULL){
        return 1;
    }
    return 0;
}