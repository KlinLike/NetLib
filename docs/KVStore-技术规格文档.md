# KVStore 技术规格文档 v1.0

## 一、项目概述

**目标**：实现一个类Redis的轻量级键值存储系统，支持多种存储引擎和网络模型。

**核心特性**：
- 3种存储引擎：数组、哈希表、红黑树
- 网络模型：Reactor（基于epoll）
- 文本协议：类Redis命令格式
- 5种基本操作：SET、GET、DEL、MOD、EXIST

---

## 二、系统架构

```
┌─────────────────────────────────────────────────┐
│                  客户端 (telnet/testcase)        │
└────────────────────┬────────────────────────────┘
                     │ TCP连接
┌────────────────────▼────────────────────────────┐
│             网络层 (reactor.c)                   │
│  ┌──────────────────────────────────────────┐  │
│  │ epoll事件循环 + 连接管理                  │  │
│  └──────────────────────────────────────────┘  │
└────────────────────┬────────────────────────────┘
                     │ 调用协议处理
┌────────────────────▼────────────────────────────┐
│            协议层 (kvstore.c)                    │
│  ┌──────────┬──────────────┬──────────────┐    │
│  │ 分词器   │ 命令识别器   │ 命令执行器   │    │
│  └──────────┴──────────────┴──────────────┘    │
└──────┬──────────┬──────────┬───────────────────┘
       │          │          │
   ┌───▼───┐  ┌──▼────┐  ┌──▼────┐
   │ Array │  │ Hash  │  │RBTree │  存储引擎层
   │Engine │  │Engine │  │Engine │
   └───────┘  └───────┘  └───────┘
```

---

## 三、协议规范

### 3.1 命令格式

```
命令 [参数1] [参数2]
```

### 3.2 支持的命令

| 命令前缀 | 引擎 | 命令 | 参数 | 响应 |
|---------|------|------|------|------|
| 无 | 数组 | SET key value | key, value | OK / EXIST / ERROR |
| 无 | 数组 | GET key | key | value / NO EXIST |
| 无 | 数组 | DEL key | key | OK / NO EXIST |
| 无 | 数组 | MOD key value | key, value | OK / NO EXIST |
| 无 | 数组 | EXIST key | key | EXIST / NO EXIST |
| R | 红黑树 | RSET/RGET/RDEL/RMOD/REXIST | 同上 | 同上 |
| H | 哈希表 | HSET/HGET/HDEL/HMOD/HEXIST | 同上 | 同上 |

**注意**：所有响应以 `\r\n` 结尾

---

## 四、数据结构定义

### 4.1 数组引擎

```c
#define KVS_ARRAY_SIZE 1024

typedef struct {
    char *key;
    char *value;
} kvs_array_item_t;

typedef struct {
    kvs_array_item_t *table;  // 固定大小数组
    int total;                // 当前元素数量
} kvs_array_t;
```

**特点**：线性查找 O(n)，适合小规模数据

### 4.2 哈希表引擎

```c
#define MAX_TABLE_SIZE 1024

typedef struct hashnode_s {
    char *key;
    char *value;
    struct hashnode_s *next;  // 链表解决冲突
} hashnode_t;

typedef struct {
    hashnode_t **nodes;       // 槽位数组
    int max_slots;
    int count;
} hashtable_t;
```

**哈希函数**：字符ASCII值求和 % size  
**冲突解决**：链地址法（头插法）  
**特点**：查找 O(1)，适合通用场景

### 4.3 红黑树引擎

```c
#define RED   1
#define BLACK 2

typedef struct rbtree_node {
    char *key;
    void *value;
    unsigned char color;
    struct rbtree_node *left;
    struct rbtree_node *right;
    struct rbtree_node *parent;
} rbtree_node;

typedef struct {
    rbtree_node *root;
    rbtree_node *nil;         // 哨兵节点
} rbtree;
```

**特点**：查找 O(log n)，支持有序遍历

### 4.4 网络连接结构

```c
#define BUFFER_LENGTH 1024

struct conn {
    int fd;
    char rbuffer[BUFFER_LENGTH];
    int rlength;
    char wbuffer[BUFFER_LENGTH];
    int wlength;
    RCALLBACK send_callback;
    union {
        RCALLBACK recv_callback;
        RCALLBACK accept_callback;
    } r_action;
};
```

---

## 五、核心接口规范

### 5.1 存储引擎接口（以数组为例，其他引擎同理）

```c
// 创建引擎
int kvs_array_create(kvs_array_t *inst);

// 设置键值对
// 返回: 0=成功, 1=已存在, <0=错误
int kvs_array_set(kvs_array_t *inst, char *key, char *value);

// 获取值
// 返回: value指针 或 NULL
char* kvs_array_get(kvs_array_t *inst, char *key);

// 删除键值对
// 返回: 0=成功, >0=不存在, <0=错误
int kvs_array_del(kvs_array_t *inst, char *key);

// 修改值
// 返回: 0=成功, >0=不存在, <0=错误
int kvs_array_mod(kvs_array_t *inst, char *key, char *value);

// 检查存在性
// 返回: 0=存在, 1=不存在
int kvs_array_exist(kvs_array_t *inst, char *key);

// 销毁引擎
void kvs_array_destory(kvs_array_t *inst);
```

### 5.2 协议处理接口

```c
// 协议处理主入口
// 输入: msg=请求消息, length=长度
// 输出: response=响应消息
// 返回: 响应长度
int kvs_protocol(char *msg, int length, char *response);

// 分词器
// 返回: token数量
int kvs_split_token(char *msg, char *tokens[]);

// 命令过滤和执行
// 返回: 响应长度
int kvs_filter_protocol(char **tokens, int count, char *response);
```

### 5.3 网络接口

```c
// 启动Reactor服务器
int reactor_start(unsigned short port, msg_handler handler);

// 回调函数类型
typedef int (*msg_handler)(char *msg, int length, char *response);
```

---

## 六、实现要求

### 6.1 内存管理

1. **所有字符串必须复制**：使用 `kvs_malloc` 分配内存
2. **引擎销毁时释放所有内存**：防止内存泄漏
3. **DEL/MOD操作需释放旧内存**

```c
// 统一的内存管理接口
void* kvs_malloc(size_t size);
void kvs_free(void *ptr);
```

### 6.2 并发支持

- 当前版本：单线程，无需考虑线程安全
- 全局变量：`global_array`、`global_hash`、`global_rbtree`
- 网络模型：Reactor（单线程事件循环）

### 6.3 错误处理

**函数返回值约定**：
- `< 0`：内部错误（内存分配失败、参数非法等）
- `= 0`：操作成功
- `> 0`：业务层错误（键已存在、键不存在等）

### 6.4 编译配置

```c
// kvstore.h
#define ENABLE_ARRAY   1    // 启用数组引擎
#define ENABLE_RBTREE  1    // 启用红黑树引擎
#define ENABLE_HASH    1    // 启用哈希表引擎
```

---

## 七、关键算法

### 7.1 哈希表插入（头插法）

```
1. 计算哈希值: idx = hash(key)
2. 检查链表中是否已存在key
3. 如果不存在:
   - 创建新节点
   - new_node->next = hash->nodes[idx]
   - hash->nodes[idx] = new_node
```

### 7.2 红黑树平衡

**插入后调整**：
1. 新节点染红色
2. 如果父节点是红色，进行旋转和重新染色
3. 最终根节点染黑色

**删除后调整**：
1. 找到后继节点替换
2. 如果删除黑色节点，进行修复
3. 通过旋转和重新染色恢复平衡

### 7.3 Reactor事件循环

```
1. epoll_create() 创建epoll实例
2. 监听socket加入epoll (EPOLLIN)
3. while(1) {
     epoll_wait() 等待事件
     if (EPOLLIN)  执行 recv_callback()
     if (EPOLLOUT) 执行 send_callback()
   }
```

---

## 八、文件组织

```
kvstore/
├── kvstore.h           # 头文件（数据结构、函数声明）
├── kvstore.c           # 协议处理主逻辑
├── kvs_array.c         # 数组引擎实现
├── kvs_hash.c          # 哈希表引擎实现
├── kvs_rbtree.c        # 红黑树引擎实现
├── reactor.c           # Reactor网络模型
├── server.h            # 网络层数据结构
├── testcase.c          # 测试程序
└── Makefile            # 编译脚本
```

---

## 九、实现步骤

### Step 1: 数组引擎（基础）
- 实现 `kvs_array.c` 的所有函数
- 编写单元测试验证

### Step 2: 哈希表引擎
- 实现哈希函数
- 实现链表操作
- 测试冲突处理

### Step 3: 红黑树引擎
- 实现旋转操作
- 实现插入修复
- 实现删除修复

### Step 4: 协议层
- 实现分词器
- 实现命令识别
- 连接三种引擎

### Step 5: 网络层
- 实现Reactor框架
- 实现连接管理
- 集成协议处理

### Step 6: 测试
- 使用telnet手动测试
- 运行testcase自动测试
- 性能压测

---

## 十、验收标准

### 10.1 功能测试

```bash
# 编译
make

# 启动服务器
./kvstore 9999

# 测试（另一个终端）
telnet localhost 9999
> SET name Alice
OK
> GET name
Alice
> DEL name
OK
```

### 10.2 自动化测试

```bash
# 编译测试程序
make testcase

# 运行测试（服务器需先启动）
./testcase 127.0.0.1 9999 0   # 红黑树测试
./testcase 127.0.0.1 9999 2   # 数组测试
./testcase 127.0.0.1 9999 3   # 哈希表测试
```

### 10.3 性能指标

- **并发连接数**：支持1024+
- **QPS（单线程）**：10000+
- **内存占用**：< 100MB（空载）

### 10.4 代码质量

```bash
# 无编译警告
gcc -Wall -Wextra

# 无内存泄漏
valgrind --leak-check=full ./kvstore 9999
```

---

## 十一、关键技术点

### 11.1 strtok陷阱
- **会修改原字符串**（将分隔符替换为\0）
- **不可重入**（使用静态变量）
- **解决方案**：传入前复制字符串

### 11.2 命令查找优化
- **当前**：线性查找 O(n)
- **优化方向**：使用哈希表映射命令字符串到枚举值

### 11.3 红黑树NIL节点
- **作用**：简化边界条件判断
- **特点**：所有叶子节点的left/right指向nil
- **注意**：nil节点必须是黑色

### 11.4 Reactor的fd复用
- **conn_list数组索引即为fd**
- **前提**：fd值不会超过CONNECTION_SIZE
- **优点**：O(1)查找连接

---

## 十二、扩展方向

1. **持久化**：RDB快照、AOF日志
2. **多线程**：主从Reactor、线程池
3. **更多引擎**：跳表、B+树
4. **更多命令**：INCR、EXPIRE、MGET等
5. **协议升级**：支持二进制协议
6. **集群支持**：一致性哈希、主从复制