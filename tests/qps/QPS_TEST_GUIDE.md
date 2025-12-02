# QPS 性能测试指南

完成 C1000K（百万连接）测试后，下一步是测试服务器的 **QPS（每秒查询数）**。

## C1000K vs QPS

| 指标 | 说明 | 关注点 |
|------|------|--------|
| **C1000K** | 同时维持 100 万个连接 | 连接数、内存、文件描述符 |
| **QPS** | 每秒处理多少请求 | 吞吐量、延迟、CPU |

## 快速开始

### 编译

```bash
make qps        # 编译 QPS 客户端
make            # 编译服务器
```

### 基本测试

```bash
# 测试 Hash 数据结构（默认）
./build/qps_client -h <服务器IP> -p 2000-2019 -c 1000 -d 30 -t 8

# 测试 RBTree 数据结构
./build/qps_client -h <服务器IP> -p 2000-2019 -c 1000 -e rbtree -d 30

# 测试 Array 数据结构
./build/qps_client -h <服务器IP> -p 2000-2019 -c 1000 -e array -d 30
```

### 双虚拟机测试（推荐）

**服务器端（VM1）**:
```bash
# 1. 配置系统参数（高并发测试需要）
sudo ./tests/setup_system.sh auto

# 2. 确保 BUF_LEN >= 1024（include/server.h）
# 3. 编译并启动服务器
make
./build/server 2000 20  # 监听 2000-2019 共 20 个端口
```

**客户端（VM2）**:
```bash
# 1. 编译 QPS 客户端
make qps

# 2. 运行测试
./build/qps_client -h 192.168.x.x -p 2000-2019 -c 1000 -e hash -d 30
```

## 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-h, --host` | 服务器地址 | 127.0.0.1 |
| `-p, --port` | 端口或端口范围（如 2000 或 2000-2019） | 2000 |
| `-c, --connections` | 总连接数 | 100 |
| `-d, --duration` | 测试时长（秒） | 30 |
| `-t, --threads` | 工作线程数 | 4 |
| `-e, --engine` | 数据结构：`hash` / `rbtree` / `array` | hash |

## 测试流程

1. **预热阶段**：每个线程写入 1000 个 key-value（使用 HSET/RSET/SET）
2. **测试阶段**：只读取已有的 key（使用 HGET/RGET/GET 和 HEXIST/REXIST/EXIST）

## 批量测试示例

### 对比不同数据结构

```bash
for engine in hash rbtree array; do
    echo "=== Testing $engine ==="
    ./build/qps_client -h 192.168.x.x -p 2000-2019 -c 1000 -e $engine -d 20
    sleep 2
done
```

### 对比不同连接数

```bash
for conn in 100 1000 10000; do
    echo "=== Testing $conn connections ==="
    ./build/qps_client -h 192.168.x.x -p 2000-2019 -c $conn -e hash -d 20
    sleep 2
done
```

### 完整基准测试

```bash
for engine in hash rbtree array; do
    for conn in 100 1000 5000; do
        echo "=== $engine / $conn connections ==="
        ./build/qps_client -h 192.168.x.x -p 2000-2019 -c $conn -e $engine -d 15
        sleep 2
    done
done
```

## 输出示例

```
======================================================
           QPS 性能测试客户端
======================================================
服务器:      192.168.1.100:2000-2019 (20 ports)
数据结构:    Hash
连接数:      1000
线程数:      4
测试时长:    30 秒
======================================================

[Thread 0] Warming up: writing 1000 keys using HSET...
[Thread 0] Warmup done: 1000/1000 keys written
...

Time(s)         Total      Success       Failed          QPS      Lat(us)
------------------------------------------------------------------------
1               22000        22000            0        22000          180
2               44500        44500            0        22500          178
3               67200        67200            0        22700          176
...

======================================================
                   测试结果
======================================================
总请求数:    675000
成功请求:    675000
失败请求:    0
成功率:      100.00%
平均QPS:     22500.00
平均延迟:    177.50 us
======================================================
```

## 性能调优建议

### 客户端调优

1. **增加线程数**: 线程数一般设为 CPU 核数的 1-2 倍
   ```bash
   ./qps_client --threads 16
   ```

2. **调整连接数**: 连接太少可能无法充分利用服务器，太多可能导致客户端瓶颈
   ```bash
   ./qps_client --connections 20000
   ```

3. **多客户端并行**: 单机客户端能力有限时，可启动多个客户端实例
   ```bash
   # 客户端1
   ./qps_client --connections 25000 --threads 4
   # 客户端2（另一台机器）
   ./qps_client --connections 25000 --threads 4
   ```

### 服务器调优

1. **CPU 亲和性**: 将服务器进程绑定到特定 CPU 核
   ```bash
   taskset -c 0-7 ./build/server 2000 100
   ```

2. **内核参数**: 确保已运行 `setup_system.sh` 优化系统参数

3. **监控资源**: 测试时监控 CPU、内存、网络
   ```bash
   # 另一个终端
   top -p $(pgrep -f "build/server")
   sar -n DEV 1
   ```

## 常见问题

### 1. QPS 很低

- 检查网络带宽是否饱和
- 检查 CPU 是否满载
- 尝试增加客户端线程数
- 尝试使用多台客户端

### 2. 大量请求失败

- 检查服务器是否正常运行
- 检查端口范围是否正确
- 降低连接数或请求速率

### 3. 延迟很高

- 检查网络延迟：`ping <服务器IP>`
- 检查服务器 CPU 是否过载
- 考虑减少连接数

## 与 C1000K 测试的区别

| 测试 | 工具 | 目的 |
|------|------|------|
| C1000K | `stress_client` | 建立并**维持**大量连接 |
| QPS | `qps_client` | 持续**发送请求**并统计吞吐量 |

C1000K 测试证明系统能维持百万连接，QPS 测试则验证在这些连接上能处理多少实际业务。

## 进阶：同时测试 C1000K + QPS

可以先用 `stress_client` 建立大量空闲连接（模拟真实场景），然后用 `qps_client` 测试活跃连接的吞吐量：

```bash
# 终端1：维持 50 万空闲连接
./build/stress_client --connections 500000 --port-range 2000-2049

# 终端2：1 万活跃连接进行 QPS 测试
./build/qps_client --connections 10000 --port-range 2050-2099
```
