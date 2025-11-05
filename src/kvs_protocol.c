#include "kvstore.h"
#include "kvs_protocol.h"
#include <string.h>
#include <stdio.h>

// ----- 协议解析器实现 -----
/* NOTE:
 * strtok不会分配新内存,而是直接修改原字符串:
 * 
 * 调用前: msg = "GET key value"
 *         内存: ['G']['E']['T'][' ']['k']['e']['y'][' ']['v']['a']['l']['u']['e']['\0']
 * 
 * 调用后: msg = "GET\0key\0value\0"
 *         内存: ['G']['E']['T']['\0']['k']['e']['y']['\0']['v']['a']['l']['u']['e']['\0']
 *                 ↑              ↑              ↑
 *         tokens[0]          tokens[1]      tokens[2]
 */

// 分词器 - 将字符串按空格分割成多个token
int kvs_tokenizer(char* msg, char** tokens){
    if(msg == NULL || tokens == NULL){
        return KVS_ERR_PARAM;
    }
    int idx = 0;
    char* token = strtok(msg, " ");
    while(token != NULL){
        tokens[idx++] = token;
        token = strtok(NULL, " "); // 后续调用传入NULL，继续分割
    }
    return idx;
}

// 命令字符串数组（实现细节，不对外暴露）
static const char *command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST",        // 数组
	"RSET", "RGET", "RDEL", "RMOD", "REXIST",   // 红黑树
	"HSET", "HGET", "HDEL", "HMOD", "HEXIST"    // 哈希表
};

// TODO: 考虑是否应该将命令识别器和命令执行器合并为一个函数?
//       当前设计: 分离的识别器和执行器
//       优点: 职责分离,便于测试和维护
//       缺点: 增加了函数调用开销

// 命令识别器 - 识别命令并返回命令索引
int kvs_parser_command(char** tokens){
    if(tokens == NULL){
        return KVS_ERR_PARAM;
    }
    
    int cmd = KVS_CMD_START;
    for(cmd = KVS_CMD_START; cmd < KVS_CMD_COUNT; cmd++){
        if(strcmp(tokens[0], command[cmd]) == 0){
            return cmd;
        }
    }
    return KVS_ERR_PARAM;
}

// 命令执行器
int kvs_executor_command(int cmd, char** tokens, char* response){
    if(cmd < KVS_CMD_START || cmd >= KVS_CMD_COUNT){
        return KVS_ERR_PARAM;
    }

    int ret = KVS_OK;
    char* key = tokens[1];
    char* value = tokens[2];

    switch(cmd){
        case KVS_CMD_SET:
            ret = kvs_array_set(global_array, key, value);
            if(ret == KVS_OK){
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_GET:
            ret = kvs_array_get(global_array, key, &value);
            if(ret == KVS_OK){
                sprintf(response, "OK %s", value);
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_DEL:
            ret = kvs_array_del(global_array, key);
            if(ret == KVS_OK){
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_MOD:
            ret = kvs_array_mod(global_array, key, value);
            if(ret == KVS_OK){
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_EXIST:
            ret = kvs_array_exist(global_array, key);
            if(ret == KVS_OK){
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_RSET:
            ret = kvs_rbtree_set(global_rbtree, key, value);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_RGET:
            ret = kvs_rbtree_get(global_rbtree, key, &value);
            if (ret == KVS_OK) {
                sprintf(response, "OK %s", value);
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_RDEL:
            ret = kvs_rbtree_del(global_rbtree, key);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_RMOD:
            ret = kvs_rbtree_mod(global_rbtree, key, value);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_REXIST:
            ret = kvs_rbtree_exist(global_rbtree, key);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_HSET:
            ret = kvs_hash_set(global_hash, key, value);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_HGET:
            ret = kvs_hash_get(global_hash, key, &value);
            if (ret == KVS_OK) {
                sprintf(response, "OK %s", value);
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_HDEL:
            ret = kvs_hash_del(global_hash, key);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_HMOD:
            ret = kvs_hash_mod(global_hash, key, value);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        case KVS_CMD_HEXIST:
            ret = kvs_hash_exist(global_hash, key);
            if (ret == KVS_OK) {
                sprintf(response, "OK");
            } else {
                sprintf(response, "%s", kvs_strerror(ret));
            }
            break;
        default:
            sprintf(response, "ERROR: Unknown command");
            return KVS_ERR_PARAM;
    }
    return KVS_OK;
}

