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

# 后续计划（2025-11-16）

1. 增加持久化保存
   - 提供简单的快照（RDB）与追加日志（AOF）两种模式
   - 在协议层增加 `SAVE`/`LOAD` 命令或服务端定时触发
   - 写入采用顺序追加、读入构建内存结构；异常恢复优先 AOF 回放

2. 预留 HTTP 与 WebSocket 接入
   - 利用 Reactor 的低耦合设计，在 `reactor_mainloop` 通过注册不同 `msg_handler` 支持多协议
   - HTTP：最小化路由与响应（GET/POST），复用 KVS 命令作为 REST 接口
   - WebSocket：长连接与消息帧解析，转发到统一的 KVS `msg_handler`
   - 模块边界：网络层仅负责 I/O 与连接生命周期，协议层负责解析与执行，保持清晰分层

3. 针对 KVS 进行性能测试
   - 建立端到端性能基准：吞吐（QPS）、延迟分布（P50/P95/P99）
   - 命令覆盖：Array/RBTree/Hash 的 `SET/GET/MOD/DEL/EXIST` 横向对比
   - 规模与并发：不同数据规模与并发连接数的性能曲线
   - 报告输出：CSV/Markdown 结果与可视化脚本，支持回归比较

