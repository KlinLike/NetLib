# NetLib 项目结构

## 📁 目录结构

```
NetLib/
├── build/              # 编译输出目录
│   ├── *.o            # 目标文件
│   └── echo_server    # 可执行文件
├── docs/              # 文档目录
│   ├── LICENSE        # 开源协议
│   ├── README.md      # 项目说明
│   ├── LOGGER_USAGE.md      # 日志系统使用文档
│   └── PROJECT_STRUCTURE.md # 本文件
├── include/           # 头文件目录
│   ├── logger.h       # 日志系统
│   ├── server.h       # 服务器通用定义
│   ├── kvstore.h      # KV存储接口
│   ├── hash.h         # 哈希表
│   └── kvs_array.h    # 数组实现
├── src/               # 源代码目录
│   ├── reactor.c      # Reactor事件循环（核心）
│   ├── kvstore.c      # KV存储主程序
│   ├── hash.c         # 哈希表实现
│   ├── kvs_array.c    # 数组实现
│   └── kvs_rbtree.c   # 红黑树实现
├── tests/             # 测试工具目录
│   └── echo_client.py # Echo客户端测试工具
├── Makefile           # 构建脚本
└── .gitignore         # Git忽略规则
```

## 📚 核心文件说明

### 源代码 (src/)

| 文件 | 说明 | 状态 |
|------|------|------|
| `reactor.c` | 基于epoll的Reactor模式事件循环，支持高并发连接 | ✅ 完成 |
| `kvstore.c` | KV存储服务主程序，包含main函数入口 | 🚧 开发中 |
| `hash.c` | 哈希表数据结构实现 | 📝 待实现 |
| `kvs_array.c` | 基于数组的KV存储实现 | 📝 待实现 |
| `kvs_rbtree.c` | 基于红黑树的KV存储实现 | 📝 待实现 |

### 头文件 (include/)

| 文件 | 说明 |
|------|------|
| `logger.h` | 统一日志系统，支持多级别彩色日志输出 |
| `server.h` | 服务器通用定义，连接结构体等 |
| `kvstore.h` | KV存储接口定义 |
| `hash.h` | 哈希表接口 |
| `kvs_array.h` | 数组存储接口 |

### 测试工具 (tests/)

| 文件 | 说明 |
|------|------|
| `echo_client.py` | Python编写的Echo测试客户端，支持单次和连续测试 |

## 🔨 构建系统

### Makefile 目标

| 目标 | 说明 |
|------|------|
| `make` | 编译项目（DEBUG模式，显示所有日志）|
| `make clean` | 清理编译产物 |
| `make release` | 编译发布版本（隐藏DEBUG日志，O2优化）|
| `make run` | 编译并启动服务器在8888端口 |
| `make install` | 安装到 /usr/local/bin/ |

### 编译产物

所有编译产物都输出到 `build/` 目录：
- `build/*.o` - 目标文件
- `build/echo_server` - 最终可执行文件

## 🎯 设计原则

### 1. 目录职责单一
- `src/` - 只存放 `.c` 源文件
- `include/` - 只存放 `.h` 头文件
- `build/` - 只存放编译产物（被git忽略）
- `docs/` - 只存放文档
- `tests/` - 只存放测试工具

### 2. 模块化设计
- 每个功能模块独立的 `.c` 和 `.h` 文件
- 通过头文件暴露接口
- 便于单元测试和维护

### 3. 构建系统清晰
- 使用 `-Iinclude` 统一头文件搜索路径
- 所有编译产物集中在 `build/` 目录
- 支持增量编译，提高开发效率

## 🚀 快速开始

### 1. 编译项目
```bash
make
```

### 2. 运行服务器
```bash
./build/echo_server 8888
```

或使用快捷命令：
```bash
make run
```

### 3. 测试连接
```bash
python3 tests/echo_client.py 127.0.0.1 8888
```

## 📝 开发指南

### 添加新的源文件

1. **创建源文件**：`src/new_module.c`
2. **创建头文件**：`include/new_module.h`
3. **更新 Makefile**：
   ```makefile
   SRCS = $(SRC_DIR)/reactor.c $(SRC_DIR)/kvstore.c $(SRC_DIR)/new_module.c
   OBJS = $(BUILD_DIR)/reactor.o $(BUILD_DIR)/kvstore.o $(BUILD_DIR)/new_module.o
   
   $(BUILD_DIR)/new_module.o: $(SRC_DIR)/new_module.c $(INC_DIR)/new_module.h
       $(CC) $(CFLAGS) -c $< -o $@
   ```

### 包含头文件

由于使用了 `-Iinclude` 编译选项，包含头文件时直接使用文件名：

```c
#include "logger.h"      // 正确
#include "server.h"      // 正确
// 不要使用 #include "../include/logger.h"
```

## 🔍 依赖关系

```
reactor.c
  ├── server.h
  ├── logger.h
  └── sys/epoll.h (系统)

kvstore.c
  ├── kvstore.h
  ├── server.h
  └── logger.h

logger.h (无依赖)
server.h (无依赖)
```

## 📊 文件统计

- 源文件 (.c): 5 个
- 头文件 (.h): 5 个
- 测试工具: 1 个
- 文档: 4 个

---

**维护者**: NetLib 项目组  
**最后更新**: 2025-10-24

