# 已知问题记录

## Reactor 事件分发与连接类型区分
- 使用 `union` 存储 `accept_cb/recv_cb`，客户端套接字注册的是 `recv_cb`，但事件循环以是否存在 `accept_cb` 判定并调用，导致客户端套接字上错误调用 `accept()`：`src/reactor.c:47`, `src/reactor.c:219-224`
- 监听套接字与客户端套接字的区分逻辑需要修正，否则将出现接受连接失败与读事件无法触达 `recv_cb`

## 构建系统与目标不一致
- 目标名为 `build/echo_server`，但实际入口在 `src/kvstore.c:127-142`，启动的是 KVStore 服务器
- 未编译协议与引擎相关源文件，可能导致链接期未定义引用：`src/kvs_protocol.c`, `src/kvs_base.c`, `src/kvs_array.c`, `src/kvs_rbtree.c`, `src/kvs_hash.c`；当前 `Makefile:22-23`
- Hash 依赖 `pthread`，链接时应包含 `-pthread` 以满足 `pthread_mutex_*`：`src/kvs_hash.c:104-110`
- `make run` 在 `README.md:62-67` 出现，但 `Makefile` 未提供该目标
- `Makefile` 中 `reactor.o` 规则存在重复编译行，需清理：`Makefile:33-37`

## 端口与运行文档不一致
- README 示例使用 `8888` 端口：`README.md:33-41`；KVStore 默认端口为 `2000`：`src/kvstore.c:128-135`

## Echo 处理器与回调接口不匹配
- Echo 使用 `struct conn*` 级别的 `echo_handle/echo_encode`：`src/echo.c:7-30`, `src/echo.c:32-46`
- Reactor 当前使用消息级 `msg_handler(char*, int, char*)`：`include/server.h:13-20`, `docs/KVStore-技术规格文档.md:211`
- 缺少 Echo 到 `msg_handler` 的适配器或独立入口，导致 Echo 路径不可直接运行

## 文档链接与文件名大小写/拼写
- README 链接 `docs/PROJECT_STRUCTURE.md`，实际文件为 `docs/Project_Structure.md`：`README.md:24`, `docs/Project_Structure.md:1`
- README 链接 `docs/LOGGER_USAGE.md`，实际文件为 `docs/Logger_Uessage.md`（拼写为 Uessage）：`README.md:57-58`, `docs/Logger_Uessage.md:1`

## 连接数组索引与文件描述符上限
- 使用 `conn_list[fd]` 直接索引，若 `fd >= CONN_MAX(1024)` 将越界：`src/reactor.c:12`, `src/reactor.c:15`, `src/reactor.c:198-201`

## 协议响应与缓冲区长度
- `kvs_protocol.c` 多处使用 `sprintf` 写入 `response`，未显式限制响应长度，存在超过 `BUF_LEN(1024)` 的风险：`src/kvs_protocol.c:75-200`, `include/server.h:6`

## 初始化与宏配置一致性
- `kvs_init` 在不同宏组合下变量声明与返回路径不完整，可能编译错误：`src/kvstore.c:87-115`

## 内存管理一致性（Array 引擎）
- `kvs_array_*` 混用 `malloc/free` 与 `kvs_malloc/kvs_free`，应统一以便后续替换策略：`src/kvs_array.c:20, 36, 72-87, 140-144, 165-172`

## 测试文档与实际文件不一致
- 测试文档中提到的脚本未出现在仓库：如 `test_kvs_rbtree.sh`, `test_kvs_hash.sh`, `test_all_kvs.sh`；当前目录包含的文件见 `tests/README.md:1-35` 与实际 `tests` 列表

---

以上问题暂不处理，后续如需逐项修复，可按优先级从 Reactor 事件分发与构建系统一致性开始。