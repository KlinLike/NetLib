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

# 下一阶段计划：网络层 Reactor 整合（2025-11-08） — 状态：已完成（2025-11-16）

1. **宏配置与接口整理**
   - 调整 `include/server.h` 中的功能宏，仅保留 `ENABLE_KVSTORE`，关闭其它协议宏。
   - 统一 `reactor.c` 对 `msg_handler` 的引用，减少对具体业务实现的编译期依赖。
2. **协议层到存储层打通**
   - 在 `src/kvstore.c` 完成 `kvs_handler`，通过 `kvs_split_token`/`kvs_filter_protocol` 将命令转发到数组引擎。
   - 完善 `kvs_handle` 与 `kvs_encode`，形成“解析 → 执行 → 编码”闭环，确保响应写回 `conn->wbuff`。
3. **Reactor 主循环调整**
   - 在 `reactor_mainloop` 中缓存传入的 `handler`，`recv_cb` 根据宏调用处理函数，判定返回值决定是否注册 `EPOLLOUT`。
   - 在 `send_cb` 中调用统一的 encode 函数，发送成功后切换回 `EPOLLIN`，错误时清理连接与 epoll 事件。
4. **构建流程同步**
   - 更新 `Makefile`：默认目标仅链接 KVStore 相关源文件，`make run` 启动 `kvstore` 主程序。
5. **验证与回归**
   - 先跑 `tests/test_kvs_array.c`、`tests/test_kvs_parser.c` 确认底层功能；再通过 `telnet` 或自定义脚本执行端到端测试。
6. **后续扩展**
   - 引入哈希表、红黑树引擎后扩展命令路由；必要时恢复 `ENABLE_ECHO` 等宏并实现多协议调度。
7. **远期目标**
   - 完成 Reactor 与消息处理的解耦，最终支持 Echo / HTTP / WebSocket / KVStore 等多协议共存，通过配置选择或运行时路由实现统一网络层。
8. **设计原则说明**
   - Reactor 层仅负责监听、读写、连接生命周期管理，保证网络与业务解耦。
   - 协议模块实现统一的 `msg_handler` 接口，自行完成“解析→执行→编码”，可以独立测试和替换。
   - 未来新增协议时，可在上层通过注册或路由机制组合多个 handler，网络循环无需再次修改。
# 完成情况（2025-11-16）

- 已统一 `msg_handler` 接口并关闭非必要协议宏（仅启用 `ENABLE_KVSTORE`）。
- Reactor 主循环使用全局 `msg_handler`，规范 `recv → 业务处理 → send` 流程。
- `kvs_init` 增加内存清零与错误处理，保证三类引擎可按需初始化。
- Makefile 扩展链接 KVS 源文件并提供 `reactor-test` 与 `run` 目标。
- 新增端到端测试脚本 `tests/kvs_reactor_client.py`，支持命名空间与断言批量用例。
- 文档与测试说明更新，补充 KVS+Reactor 集成的使用方式。

## 后续计划（2025-11-16）

### KVS 性能测试（优先级：高）
- 建立端到端性能基准：吞吐（QPS）与延迟分布（P50/P95/P99）。
- 命令覆盖：Array/RBTree/Hash 的 `SET/GET/MOD/DEL/EXIST` 横向对比。
- 规模与并发：不同数据规模与并发连接数的性能曲线。
- 报告输出：CSV/Markdown 结果与可视化脚本，支持回归比较。

### HTTP 与 WebSocket 接入（优先级：中）
- 在 `reactor_mainloop` 通过注册不同 `msg_handler` 支持多协议，保持低耦合。
- HTTP：最小路由与响应（GET/POST），可将 KVS 命令映射为 REST 接口。
- WebSocket：完成握手与帧解析，消息转发至统一的 KVS 处理逻辑。
- 模块边界：网络层仅负责 I/O 与连接生命周期，协议层负责解析与执行。

#### 协议路由与回调细化（设计草案）
- 统一分发器：回调内根据协议类型分支到 HTTP/WS/KVS 的处理函数。
- 自动判定：`detect_protocol(msg,len)` → `PROTO_HTTP | PROTO_WS | PROTO_KVS`；首包判定后缓存到连接结构。
- 判定规则：
  - HTTP：首行含方法/版本，头以 `\r\n` 分隔。
  - WebSocket：HTTP 握手头含 `Upgrade: websocket`、`Connection: Upgrade`、`Sec-WebSocket-Key`。
  - KVS：首 token 属于指令集（`SET/GET/DEL/MOD/EXIST`、`R*`、`H*`）。

### 持久化保存（优先级：低）
- 快照（RDB）与追加日志（AOF）两种模式，支持定时或命令触发 `SAVE/LOAD`。
- 写入顺序追加、读入重建内存结构；异常恢复优先 AOF 回放。

### 协议路由与回调细化（设计草案）

1. 回调分支（在统一回调内选择业务处理）
   - 在网络回调中按协议类型分支：HTTP / WebSocket / KVS。
   - 统一签名：`int handler(char *msg, int len, char *resp)`，保持 `reactor_mainloop` 的解耦。
   - 代码位置参考：`src/reactor.c:85-129`（接收并写入 `conn_list[fd].wbuff`，随后触发发送）。

2. 自动解析并路由（首次消息判定后缓存协议类型）
   - 解析函数：`detect_protocol(char *msg, int len)` → 返回 `PROTO_HTTP | PROTO_WS | PROTO_KVS`。
   - 判定规则：
     - HTTP：首行包含 `HTTP/1.1` 或方法关键字（`GET `、`POST `、`PUT `等），以 `\r\n` 分隔头。
     - WebSocket：HTTP 握手，且头包含 `Upgrade: websocket`、`Connection: Upgrade`、`Sec-WebSocket-Key`。
     - KVS：首 token 命中指令集（`SET/GET/DEL/MOD/EXIST`、`R*`、`H*`）。
   - 首包解析后在连接结构体上缓存协议类型，避免每次重复解析（建议新增 `conn.protocol` 字段）。

3. 分支处理流程
   - HTTP：解析请求行与头，路由到最小 REST 接口（如 `/kvs/set?k=v`），生成文本响应。
   - WebSocket：完成握手（返回 `101 Switching Protocols`），后续按帧格式收发消息，解包后转发到 KVS 处理。
   - KVS：保留现有文本协议，按空格分词并执行命令，响应统一追加 `CRLF`。

4. 集成原则
   - Reactor 层仅负责 I/O、连接管理与事件切换；协议层负责解析与业务执行。
   - 通过一个统一的分发器在网络回调内选择具体 `handler`，确保低耦合、易扩展。
   - 预留未来：可按端口或路径进行静态路由；也可在首次包自动判定后固定协议。

