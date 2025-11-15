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

# 下一阶段计划：网络层 Reactor 整合（2025-11-08）

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

