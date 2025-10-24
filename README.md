# NetLib

高性能网络库，基于 epoll 实现的 Reactor 模式，支持 Echo 服务器和 KV 存储。

## ✨ 特性

- 🚀 **高性能**: 基于 epoll 的 Reactor 事件循环
- 🎨 **彩色日志**: 多级别日志系统，支持调试和生产模式
- 🏗️ **模块化**: 清晰的目录结构和代码组织
- 🧪 **易测试**: 提供 Python 测试工具

## 📁 项目结构

```
NetLib/
├── build/      # 编译产物（自动生成）
├── docs/       # 项目文档
├── include/    # 头文件
├── src/        # 源代码
├── tests/      # 测试工具
└── Makefile    # 构建脚本
```

详细结构说明请查看 [docs/PROJECT_STRUCTURE.md](docs/PROJECT_STRUCTURE.md)

## 🚀 快速开始

### 编译
```bash
make
```

### 运行 Echo 服务器
```bash
./build/echo_server 8888
```

或使用快捷命令：
```bash
make run
```

### 测试
```bash
# 本地测试
python3 tests/echo_client.py 127.0.0.1 8888

# 远程测试
python3 tests/echo_client.py <服务器IP> 8888

# 连续测试
python3 tests/echo_client.py 127.0.0.1 8888 --continuous 10
```

## 📖 文档

- [项目结构说明](docs/PROJECT_STRUCTURE.md) - 完整的目录结构和设计说明
- [日志系统使用](docs/LOGGER_USAGE.md) - 日志系统的详细使用指南

## 🔨 构建选项

```bash
make          # 调试模式（显示所有日志）
make release  # 发布模式（隐藏DEBUG日志，O2优化）
make clean    # 清理编译产物
make run      # 编译并运行
```

## 📝 日志示例

```
[10:30:15 SERVER] Starting echo server on port 8888...
[10:30:15 SERVER] Listening on 0.0.0.0:8888
[10:30:15 SERVER] Echo server started successfully!
[10:30:20 CONN] New client connected: 127.0.0.1:54321 (fd=5)
[10:30:20 DEBUG] Received 18 bytes from fd=5
[10:30:20 DEBUG] Sent 18 bytes to fd=5
[10:30:25 INFO] Client disconnected (fd=5)
```

## 🛠️ 开发中的功能

- [ ] KV 存储实现
- [ ] 哈希表数据结构
- [ ] 红黑树数据结构
- [ ] HTTP 协议支持
- [ ] WebSocket 支持

## 📄 许可证

见 [docs/LICENSE](docs/LICENSE)

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

**项目状态**: 🚧 开发中  
**最后更新**: 2025-10-24

