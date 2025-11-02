# 实现最基础的KV存储

- 创建kvstore.h
- 实现kvs_array.c
- 创建测试文件 test_array.c
- 同步更新makefile -> sh脚本已经可以完成测试，决定先不改动Makefile


# 实现协议解析器

**协议设计（类Redis）：**
```
SET key value      -> 返回: OK / EXIST / ERROR
GET key            -> 返回: value / NO EXIST
DEL key            -> 返回: OK / NO EXIST
MOD key value      -> 返回: OK / NO EXIST
EXIST key          -> 返回: EXIST / NO EXIST
```

## 三层架构
```
客户端输入: "SET name Alice"
    ↓
┌─────────────────────────────────┐
│  第1层：分词器 (Tokenizer)      │  → ["SET", "name", "Alice"]
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│  第2层：命令识别器 (Parser)     │  → CMD_SET
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│  第3层：命令执行器 (Executor)   │  → 调用kvs_array_set()
└─────────────────────────────────┘
    ↓
生成响应: "OK\r\n"
```

