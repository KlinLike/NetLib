# NetLib 测试套件

本目录包含NetLib项目的完整测试套件，包括单元测试、协议测试、解析器测试等。

## 📁 目录结构

```
tests/
├── 📊 KVS核心测试
│   ├── test_kvs_all.c                # ⭐ KVS性能对比测试
│   └── test_kvs_all.sh               # ⭐ 测试脚本
│
├── 🌐 协议测试
│   ├── test_protocol.c               # ⭐ 协议测试
│   └── test_protocol.sh              # ⭐ 测试脚本
│
├── 🛠️ 工具
│   ├── echo_client.py                # Echo客户端测试工具
│   └── .gitignore                    # Git忽略规则
│
└── 📚 文档
    ├── README.md                     # 本文件
    └── FINAL_SUMMARY.md              # 重构总结
```

## 🚀 快速开始

### 方式一：统一性能对比测试（推荐⭐）

**这是最推荐的测试方式**，可以在一个程序中对比三种数据结构的性能：

```bash
# 使用默认配置（1000条插入，500条修改，500条删除）
./tests/test_kvs_all.sh

# 自定义测试规模
./tests/test_kvs_all.sh -i 10000 -m 5000 -d 5000

# 小规模快速测试
./tests/test_kvs_all.sh -i 100 -m 50 -d 50

# 查看帮助
./tests/test_kvs_all.sh -h
```

**特性：**
- ✅ 统一测试所有数据结构
- ✅ 自动性能对比
- ✅ 美观的表格输出
- ✅ 可配置测试规模
- ✅ 显示性能冠军
- ✅ 代码无重复

**输出示例：**
```
┌────────────┬──────────┬──────────┬──────────┬──────────┐
│ 数据结构   │  插入    │  查询    │  修改    │  删除    │
├────────────┼──────────┼──────────┼──────────┼──────────┤
│ Array      │    3.91ms │    2.66ms │    0.73ms │    0.35ms │
│ RBTree     │    0.62ms │    0.17ms │    0.13ms │    0.11ms │
│ Hash       │    0.79ms │    0.30ms │    0.25ms │    0.27ms │
└────────────┴──────────┴──────────┴──────────┴──────────┘

🏆 性能冠军:
  插入最快: RBTree (0.62ms)
  查询最快: RBTree (0.17ms)
  修改最快: RBTree (0.13ms)
  删除最快: RBTree (0.11ms)
```

### 方式二：单独测试某个数据结构

如果只想测试特定的数据结构：

```bash
# 测试Array
./tests/test_kvs_array.sh

# 测试RBTree
./tests/test_kvs_rbtree.sh

# 测试Hash
./tests/test_kvs_hash.sh

# 运行所有单独测试
./tests/test_all_kvs.sh
```

### 方式三：协议层测试（推荐⭐）

测试协议解析器和三种数据结构的协议集成：

```bash
# 统一协议测试（推荐）
./tests/test_protocol.sh
```

**特性：**
- ✅ 测试协议解析器（分词、命令识别）
- ✅ 测试Array协议集成（SET/GET/MOD/DEL/EXIST）
- ✅ 测试RBTree协议集成（RSET/RGET/RMOD/RDEL/REXIST）
- ✅ 测试Hash协议集成（HSET/HGET/HMOD/HDEL/HEXIST）
- ✅ 自动统计测试通过率

## 📊 性能对比

### 测试配置：1000条记录

| 数据结构 | 插入    | 查询    | 修改    | 删除    | 特点 |
|---------|---------|---------|---------|---------|------|
| **Array**  | 3.91ms  | 2.66ms  | 0.73ms  | 0.35ms  | 固定容量1024，超出后无法插入 |
| **RBTree** | 0.62ms  | 0.17ms  | 0.13ms  | 0.11ms  | O(log n)复杂度，平衡良好 |
| **Hash**   | 0.79ms  | 0.30ms  | 0.25ms  | 0.27ms  | 哈希冲突少时性能优秀 |

### 测试配置：10000条记录

| 数据结构 | 插入     | 查询     | 修改     | 删除     | 说明 |
|---------|---------|---------|---------|---------|------|
| **Array**  | 5.37ms  | 2.58ms  | 3.85ms  | 1.17ms  | 仅插入1024条（容量限制） |
| **RBTree** | 7.35ms  | 2.04ms  | 1.59ms  | 1.37ms  | 性能稳定，可扩展 |
| **Hash**   | 28.66ms | 23.36ms | 16.70ms | 15.23ms | 冲突增多，性能下降 |

**结论：**
- 🏆 **RBTree** 适合大规模数据，性能稳定
- ⚡ **Hash** 适合中小规模且键分布均匀的场景
- 📦 **Array** 适合小规模固定数据，实现简单

## 🔧 测试参数说明

### 统一测试参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-i N` | 插入测试数量 | 1000 |
| `-m N` | 修改测试数量 | 500 |
| `-d N` | 删除测试数量 | 500 |
| `-h` | 显示帮助信息 | - |

### 示例场景

```bash
# 快速烟雾测试（10条数据）
./tests/test_kvs_all.sh -i 10 -m 5 -d 5

# 常规测试（1000条数据）
./tests/test_kvs_all.sh

# 大规模压力测试（10万条数据）
./tests/test_kvs_all.sh -i 100000 -m 50000 -d 50000

# 只测试插入性能
./tests/test_kvs_all.sh -i 50000 -m 0 -d 0
```

## 🎯 测试内容

每个测试都包含：

### 1. 基础功能测试
- ✅ 数据结构创建/销毁
- ✅ Set（设置键值对）
- ✅ Get（获取值）
- ✅ Mod（修改值）
- ✅ Del（删除键值对）
- ✅ Exist（检查键是否存在）
- ✅ 重复键处理
- ✅ 边界条件测试

### 2. 压力测试
- 📈 批量插入
- 📈 批量查询
- 📈 批量修改
- 📈 批量删除
- 📊 性能统计（总时间、平均时间）

### 3. 性能对比（仅统一测试）
- 🏆 识别最快的数据结构
- 📊 表格化性能数据
- ⚖️ 公平的对比环境

## 🛠️ 手动编译

如果需要手动编译测试程序：

### 统一测试
```bash
gcc -o test_kvs_all \
    tests/test_kvs_all.c \
    src/kvs_base.c \
    src/kvs_array.c \
    src/kvs_rbtree.c \
    src/kvs_hash.c \
    -I./include -Wall -Wextra -pthread -g
```

### 单独测试
```bash
# Array
gcc -o test_array tests/test_kvs_array.c src/kvs_base.c src/kvs_array.c -I./include -Wall -g

# RBTree  
gcc -o test_rbtree tests/test_kvs_rbtree.c src/kvs_base.c src/kvs_rbtree.c -I./include -Wall -g

# Hash
gcc -o test_hash tests/test_kvs_hash.c src/kvs_base.c src/kvs_hash.c -I./include -Wall -pthread -g
```

## 📝 测试最佳实践

1. **开发阶段**：使用快速测试验证功能
   ```bash
   ./tests/test_kvs_all.sh -i 100
   ```

2. **提交前**：运行完整测试确保质量
   ```bash
   ./tests/test_kvs_all.sh
   ```

3. **性能调优**：使用大规模测试找瓶颈
   ```bash
   ./tests/test_kvs_all.sh -i 100000
   ```

4. **CI/CD集成**：在自动化流程中使用
   ```bash
   ./tests/test_all_kvs.sh || exit 1
   ```

## 🐛 故障排查

### 编译失败
```bash
# 检查依赖文件是否存在
ls src/kvs_*.c
ls include/kvs*.h

# 检查编译器
gcc --version
```

### 测试失败
```bash
# 查看详细错误信息（脚本会保留可执行文件）
./test_kvs_all -i 10

# 使用gdb调试
gdb ./test_kvs_all
```

### Array容量限制
Array默认容量为1024，在 `include/kvstore.h` 中修改：
```c
#define KVS_ARRAY_SIZE  1024  // 修改为更大的值
```

## 📚 更多文档

- `README_TESTS.md` - 详细的单独测试文档
- `../README.md` - 项目总体说明
- 源码注释 - 实现细节说明

## 🤝 贡献

发现bug或有改进建议？欢迎提交issue或PR！

---

**Happy Testing! 🎉**

## 🔧 Reactor 测试（Echo）

### 目的
- 独立验证 Reactor 事件循环的“接收→处理→发送”链路是否正常（不依赖 KVS 协议与存储引擎）。

### 一键使用脚本
- 构建：`./tests/test_reactor.sh build`
- 运行：`./tests/test_reactor.sh run`（端口 `8888`）
- 清理：`./tests/test_reactor.sh clean`

### 手动测试（KVS+Reactor）
- 本地：`python3 tests/kvs_reactor_client.py 127.0.0.1 2000`
- 远程：`python3 tests/kvs_reactor_client.py <服务器IP> 2000`
- 自定义前缀：`python3 tests/kvs_reactor_client.py <服务器IP> 2000 --prefix smoke`

### 预期输出
- 客户端显示连接成功并收到与发送内容一致的回显（含结尾换行符）。
- 服务器日志显示启动、监听、等待连接、收发数据等事件（取决于 `LOG_LEVEL`）。

### 说明
- 服务器入口：`src/kvstore.c`，通过 `reactor_mainloop(port, 1, kvs_handler)` 注册 `kvs_handler`。
- 运行端口默认 `2000`，可在启动时传参修改。

*最后更新：2025-11-16*
