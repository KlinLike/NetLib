#ifndef __KVS_PROTOCOL_H__
#define __KVS_PROTOCOL_H__

// 命令枚举定义
enum {
	KVS_CMD_START = 0,
	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_DEL,
	KVS_CMD_MOD,
	KVS_CMD_EXIST,
	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,
	KVS_CMD_REXIST,
	// hash
	KVS_CMD_HSET,
	KVS_CMD_HGET,
	KVS_CMD_HDEL,
	KVS_CMD_HMOD,
	KVS_CMD_HEXIST,
	
	KVS_CMD_COUNT,
};

// 分词器 - 将字符串按空格分割成多个token
int kvs_tokenizer(char* msg, char** tokens);

// 命令识别器 - 识别命令并返回命令索引
int kvs_parser_command(char** tokens);

// 命令执行器 - 执行命令并生成响应
int kvs_executor_command(int cmd, char** tokens, char* response);

#endif

