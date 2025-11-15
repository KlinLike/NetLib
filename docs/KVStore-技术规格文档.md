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
- 远期目标：Reactor 作为统一网络核心，能够同时支撑 KVStore、Echo、HTTP、WebSocket 等多种协议，通过配置或路由策略切换业务模块。

#### Step 5.1 Reactor 整合操作流程（详细）
1. **宏配置与回调接口整理**
   - 在 `server.h` 中仅启用 `ENABLE_KVSTORE`，临时关闭 `ENABLE_ECHO`、`ENABLE_HTTP`、`ENABLE_WEBSOCKET`。
   - 确保 `reactor.c` 通过统一的 `msg_handler` 处理业务逻辑，避免直接依赖 `kvs_handle` 等内部函数。
2. **协议层 → 存储层打通**
   - 在 `kvstore.c` 完成 `kvs_handler`，使用 `kvs_split_token`、`kvs_filter_protocol` 将命令映射到默认引擎（数组）。
   - 调整 `kvs_handle`、`kvs_encode`，形成“解析 → 执行 → 编码”闭环，并将响应写回 `conn->wbuff`。
3. **Reactor 主循环适配**
   - `reactor_mainloop` 内缓存传入的 `handler` 指针；`recv_cb` 调用后根据返回值决定是否注册 `EPOLLOUT`，异常时释放连接。
   - `send_cb` 负责调用 encode 函数、发送数据并切回 `EPOLLIN`，必要时移除 epoll 事件。
4. **构建与运行流程同步**
   - 更新 `Makefile`：默认目标仅链接 KVStore 相关源文件，`make run` 启动 `kvstore` 主程序。
   - 确认脚本与 README 指向新的可执行路径。
5. **验证与回归**
   - 单元测试：运行 `tests/test_kvs_array.c`、`tests/test_kvs_parser.c` 确认底层功能。
   - 端到端：启动服务后使用 `telnet` 或脚本发送命令，验证响应格式与状态码。
6. **后续扩展与回滚预案**
   - 在数组引擎稳定后再接入哈希表、红黑树，扩展命令路由。
   - 若需并行支持 Echo/HTTP，在 `struct conn` 中增加协议标识，完善多协议调度。
7. **架构选择理由**
   - Reactor 仅承担网络事件循环与连接生命周期管理，缩小模块职责，便于替换或优化网络层实现。
   - 协议模块对外暴露统一的 `msg_handler`，内部完成“解析→执行→编码”，可以独立编译与测试，提高模块复用性。
   - 当需要多协议共存时，只需在上层实现路由或注册机制，Reactor 无需修改即可接入新 handler，避免重复调试网络细节。

#### Step 5.2 事件处理链路（epoll → KVStore → epoll）
1. **事件触发**：`reactor_mainloop` 调用 `epoll_wait`，当监听 fd 收到可读事件（`EPOLLIN`）时，由 `recv_cb` 负责读取数据。
2. **数据读取**：`recv_cb` 使用 `read` 将报文填入 `conn->rbuff`，同时清空 `conn->wbuff`，准备写回缓冲区。
3. **业务处理**：如果注册了 `msg_handler`，`recv_cb` 将 `rbuff` 传入。例如启用 KVStore 时，`kvs_handler` 会执行：
   - 将原始请求复制到本地缓冲避免修改原数据；
   - 调用 `kvs_tokenizer`/`kvs_parser_command`/`kvs_executor_command`，完成“分词→识别→执行”；
   - 将响应写入 `conn->wbuff` 并追加 `\r\n`，返回响应长度。
4. **切换写事件**：`recv_cb` 判断 `conn->wbuff_len`，若大于 0，则通过 `epoll_ctl` 将 fd 改成 `EPOLLOUT`，等待发送。
5. **发送响应**：`send_cb` 在写事件就绪时调用 `write` 把 `conn->wbuff` 发送给客户端，成功后切回 `EPOLLIN` 等待下一次请求；若写失败或对端关闭，则关闭 fd 并从 epoll 中移除。
6. **生命周期管理**：整个过程中，Reactor 仅维护连接状态与事件切换，`msg_handler` 自主管理协议逻辑，实现多协议共存时只需替换或组合 handler。

### Step 6: 测试
- 使用telnet手动测试
- 运行testcase自动测试00
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

## 十二、业务逻辑与网络层的解耦

### 12.1 回调机制的本质

Reactor 网络层通过 `msg_handler` 回调接口与业务逻辑层解耦：

```c
typedef int (*msg_handler)(char *msg, int length, char *response);
```

**设计思想**：
- Reactor 仅负责网络事件的管理（何时读、何时写、连接生命周期）
- 业务逻辑层负责协议处理（如何解析、如何执行、如何编码）
- 两者通过回调接口连接，实现松耦合

### 12.2 职责分离

```
Reactor（网络层）          KVStore（业务层）
    │                          │
    ├─ 管理连接状态            ├─ 解析协议
    ├─ 事件循环                ├─ 执行命令
    ├─ 缓冲区管理              ├─ 生成响应
    └─ 生命周期管理            └─ 内存管理
```

**Reactor 的职责**：
1. **事件监听**：通过 `epoll_wait` 监听网络事件
2. **数据读取**：在 `EPOLLIN` 事件时调用 `read` 填充 `rbuffer`
3. **数据发送**：在 `EPOLLOUT` 事件时调用 `write` 发送 `wbuffer`
4. **事件切换**：根据处理结果切换 `EPOLLIN` 和 `EPOLLOUT`
5. **连接管理**：维护连接状态，处理连接关闭

**业务逻辑的职责**：
1. **协议解析**：将原始请求解析为命令和参数
2. **命令执行**：调用存储引擎执行具体操作
3. **响应生成**：将执行结果编码为协议格式
4. **内存管理**：管理业务数据的内存分配和释放

### 12.3 解耦的好处

**1. 可替换性**
- 替换 `msg_handler` 即可支持不同协议（Echo、HTTP、WebSocket）
- Reactor 代码无需修改，只需注册不同的 handler

**2. 可测试性**
- 业务逻辑可独立测试，无需启动网络服务
- 网络层可独立测试，使用 mock handler

**3. 可扩展性**
- 多协议共存只需注册多个 handler 或实现路由机制
- 新增协议不影响现有协议的处理逻辑

**4. 可维护性**
- 网络层和业务层职责清晰，便于维护和优化
- 修改业务逻辑不影响网络层，反之亦然

### 12.4 实现示例

```c
// Reactor 调用业务逻辑
int recv_cb(struct conn *conn) {
    // 1. 读取数据到 rbuffer
    int n = read(conn->fd, conn->rbuffer, BUFFER_LENGTH);
    
    // 2. 调用业务逻辑处理
    if (msg_handler) {
        int response_len = msg_handler(
            conn->rbuffer, 
            conn->rlength, 
            conn->wbuffer
        );
        conn->wlength = response_len;
    }
    
    // 3. 切换为写事件
    if (conn->wlength > 0) {
        epoll_ctl(epfd, EPOLL_CTL_MOD, conn->fd, &ev_out);
    }
}

// 业务逻辑实现
int kvs_handler(char *msg, int length, char *response) {
    // 1. 复制请求避免修改原数据
    char *local_buf = kvs_malloc(length + 1);
    memcpy(local_buf, msg, length);
    
    // 2. 分词和解析
    char *tokens[10];
    int token_count = kvs_split_token(local_buf, tokens);
    
    // 3. 执行命令
    int response_len = kvs_filter_protocol(tokens, token_count, response);
    
    // 4. 释放临时内存
    kvs_free(local_buf);
    
    return response_len;
}
```

---

## 十三、设计模式：Reactor 模式

### 13.1 Reactor 模式的核心思想

Reactor 模式是一种事件驱动的设计模式，用于处理多个并发连接，核心思想是：

**"不要阻塞等待事件，而是注册事件处理器，当事件发生时由事件分发器通知处理器"**

```
事件源（网络I/O）
    ↓
事件分发器（epoll）
    ↓
事件处理器（recv_cb, send_cb）
    ↓
业务处理器（msg_handler）
```

### 13.2 Reactor 模式的组件

**1. 事件源（Event Source）**
- 网络 socket 文件描述符
- 每个连接对应一个 fd，产生可读/可写事件

**2. 事件分发器（Event Demultiplexer）**
- `epoll` 系统调用
- 负责监听多个 fd，当事件就绪时通知应用

**3. 事件处理器（Event Handler）**
- `recv_cb`：处理读事件
- `send_cb`：处理写事件
- `accept_cb`：处理新连接事件

**4. 业务处理器（Business Handler）**
- `msg_handler`：处理业务逻辑
- 由业务层实现，网络层通过回调调用

### 13.3 Reactor 模式的工作流程

```
1. 初始化阶段
   - 创建 epoll 实例
   - 注册监听 socket 到 epoll（EPOLLIN）

2. 事件循环阶段
   while(1) {
       // 等待事件（阻塞）
       events = epoll_wait(epfd, events, MAX_EVENTS, -1);
       
       // 处理每个就绪的事件
       for (each event in events) {
           if (event == EPOLLIN) {
               // 读事件：接收数据并处理
               recv_cb(event.fd);
           } else if (event == EPOLLOUT) {
               // 写事件：发送数据
               send_cb(event.fd);
           }
       }
   }

3. 事件处理阶段
   - recv_cb: 读取数据 → 调用业务逻辑 → 切换为写事件
   - send_cb: 发送数据 → 切换为读事件
```

### 13.4 与其他模式的对比

| 模式 | 特点 | 适用场景 | 优缺点 |
|------|------|----------|--------|
| **阻塞I/O** | 一个线程处理一个连接 | 低并发场景 | 优点：简单<br>缺点：无法并发 |
| **多线程** | 每个连接一个线程 | 高并发但CPU密集 | 优点：并发能力强<br>缺点：资源消耗大，线程切换开销 |
| **Reactor** | 单线程处理多连接 | 高并发且I/O密集 | 优点：资源高效，单线程高并发<br>缺点：CPU密集任务会阻塞 |
| **Proactor** | 异步I/O，操作系统完成I/O操作 | 高并发且I/O密集 | 优点：性能最优<br>缺点：实现复杂，平台依赖 |

### 13.5 Reactor 模式的优势

**1. 资源高效**
- 单线程处理多个连接，避免线程创建和切换开销
- 内存占用小，适合高并发场景

**2. 响应及时**
- 事件驱动，事件就绪立即处理
- 无轮询开销，CPU利用率高

**3. 扩展性好**
- 通过回调机制支持多种协议
- 业务逻辑与网络层解耦，易于扩展

**4. 实现简单**
- 相比 Proactor 模式，实现复杂度较低
- 基于成熟的 epoll 机制，稳定可靠

### 13.6 Reactor 模式的局限性

**1. CPU密集任务会阻塞**
- 单线程执行，CPU密集任务会阻塞事件循环
- 解决方案：使用线程池处理CPU密集任务

**2. 编程复杂度**
- 需要维护连接状态和事件切换
- 错误处理相对复杂

**3. 调试困难**
- 异步回调导致调用栈不直观
- 需要良好的日志和调试工具

### 13.7 Reactor 模式的变体

**1. 单Reactor单线程**
- 当前实现方式
- 适合I/O密集、CPU不密集的场景

**2. 单Reactor多线程**
- Reactor 负责网络I/O
- 线程池负责业务处理
- 适合CPU密集任务

**3. 主从Reactor多线程**
- 主Reactor负责接受连接
- 从Reactor负责处理连接
- 适合超高并发场景

---

## 十四、扩展方向

1. **持久化**：RDB快照、AOF日志
2. **多线程**：主从Reactor、线程池
3. **更多引擎**：跳表、B+树
4. **更多命令**：INCR、EXPIRE、MGET等
5. **协议升级**：支持二进制协议
6. **集群支持**：一致性哈希、主从复制