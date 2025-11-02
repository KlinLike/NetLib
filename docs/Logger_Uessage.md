# 日志系统使用指南

## 📚 简介

NetLib 提供了一个简单易用的彩色日志系统，支持多种日志级别和自动时间戳。

## 🎨 日志级别

| 级别 | 宏函数 | 颜色 | 用途 |
|------|--------|------|------|
| DEBUG | `log_debug()` | 灰色 | 调试信息，开发时使用 |
| INFO | `log_info()` | 绿色 | 一般信息，如客户端断开连接 |
| WARN | `log_warn()` | 黄色 | 警告信息，不影响正常运行 |
| ERROR | `log_error()` | 红色 | 错误信息，需要关注 |
| CONN | `log_connection()` | 青色 | 连接相关的特殊日志 |
| SERVER | `log_server()` | 品红色 | 服务器启动相关日志 |

## 💻 使用方法

### 1. 包含头文件

```c
#include "logger.h"
```

### 2. 使用日志函数

```c
// DEBUG 日志 - 调试信息
log_debug("Connection fd=%d, buffer size=%d", fd, size);

// INFO 日志 - 一般信息
log_info("Client disconnected (fd=%d)", fd);

// WARN 日志 - 警告
log_warn("Buffer almost full: %d/%d bytes", used, total);

// ERROR 日志 - 错误
log_error("Failed to bind port %d: %s", port, strerror(errno));

// CONN 日志 - 连接事件
log_connection("New client from %s:%d", ip, port);

// SERVER 日志 - 服务器事件
log_server("Listening on 0.0.0.0:%d", port);
```

## 🔧 编译配置

### 开发模式 (显示所有日志)

```bash
make clean && make
```

### 生产模式 (隐藏 DEBUG 日志)

```bash
make release
```

### 自定义日志级别

```bash
# 只显示 WARN 和 ERROR
make clean
make CFLAGS="-Wall -g -DLOG_LEVEL=2"

# 只显示 ERROR
make clean
make CFLAGS="-Wall -g -DLOG_LEVEL=3"
```

## 📊 输出示例

### 开发模式输出

```
[10:30:15 SERVER] Starting echo server on port 8888...
[10:30:15 DEBUG] epoll_create success, epfd=3
[10:30:15 DEBUG] set_epoll_event: epfd=3, fd=4, event=0x1, ADD
[10:30:15 SERVER] Listening on 0.0.0.0:8888
[10:30:15 SERVER] Echo server started successfully!
[10:30:15 SERVER] Waiting for connections...
[10:30:20 DEBUG] epoll_wait returned 1 events
[10:30:20 DEBUG] Event on fd=4, events=0x1
[10:30:20 CONN] New client connected: 127.0.0.1:54321 (fd=5)
[10:30:20 DEBUG] Received 18 bytes from fd=5
[10:30:20 DEBUG] Sent 18 bytes to fd=5
[10:30:25 INFO] Client disconnected (fd=5)
```

### 生产模式输出 (make release)

```
[10:30:15 SERVER] Starting echo server on port 8888...
[10:30:15 SERVER] Listening on 0.0.0.0:8888
[10:30:15 SERVER] Echo server started successfully!
[10:30:15 SERVER] Waiting for connections...
[10:30:20 CONN] New client connected: 127.0.0.1:54321 (fd=5)
[10:30:25 INFO] Client disconnected (fd=5)
```

## 🎯 最佳实践

### 1. 选择合适的日志级别

```c
// ✅ 正确：使用 DEBUG 记录详细的调试信息
log_debug("Processing packet: type=%d, size=%d", type, size);

// ✅ 正确：使用 INFO 记录正常的运行信息
log_info("Configuration reloaded successfully");

// ✅ 正确：使用 WARN 记录潜在问题
log_warn("Connection pool usage: %d%%", usage);

// ✅ 正确：使用 ERROR 记录错误
log_error("Database connection failed: %s", error_msg);
```

### 2. 包含有用的上下文信息

```c
// ❌ 不好：信息不足
log_error("Failed");

// ✅ 好：包含详细信息
log_error("Failed to connect to database %s:%d: %s", 
          host, port, strerror(errno));
```

### 3. 使用 strerror(errno) 显示系统错误

```c
// ❌ 不好：只显示 errno 数字
log_error("bind failed, errno=%d", errno);

// ✅ 好：显示可读的错误信息
log_error("bind failed: %s", strerror(errno));
```

## 🚀 性能优化

### 自动优化

日志系统已经内置了以下优化：

1. **编译时优化**：设置高日志级别后，低级别日志会被完全编译掉，零开销
2. **自动刷新**：每条日志自动调用 `fflush(stdout)`，无需手动刷新
3. **时间戳缓存**：每条日志只调用一次 `time()` 和 `localtime()`

### 性能影响

- **DEBUG 模式**：每次事件约增加 1-2 微秒开销
- **INFO 模式**：每次事件约增加 0.5 微秒开销
- **生产模式** (make release)：几乎无性能影响

## 🔌 扩展日志系统

### 添加新的日志级别

编辑 `logger.h`，参考现有的宏定义：

```c
#define log_custom(fmt, ...) do { \
    char ts[32]; \
    get_timestamp(ts, sizeof(ts)); \
    printf(COLOR_BLUE "[%s CUSTOM] " fmt COLOR_RESET "\n", ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
```

## 📝 总结

- ✅ 开发时使用默认编译 `make`，可以看到所有 DEBUG 信息
- ✅ 生产环境使用 `make release`，隐藏 DEBUG 日志
- ✅ 所有日志自动带时间戳和颜色，易于区分
- ✅ 零配置，包含头文件即可使用
- ✅ 编译时优化，生产环境无性能损失

---

**最后更新**: 2025-10-24

