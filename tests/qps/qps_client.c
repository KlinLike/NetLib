// QPS 性能测试客户端
// 功能：连接到远程 KVS 服务器，持续发送请求，统计 QPS
// 用法：./qps_client --host <IP> --port <PORT> --connections <N> --duration <S>
//
// 协议格式（参考 kvs_reactor_client.py）：
//   请求: "HSET key value\r\n" 或 "HGET key\r\n"
//   响应: "OK [value]\r\n" 或 "ERROR: ...\r\n"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <stdatomic.h>
#include <signal.h>

#define BUF_SIZE 512

// 全局统计（原子操作）
static atomic_ullong g_total_requests = 0;
static atomic_ullong g_success_requests = 0;
static atomic_ullong g_failed_requests = 0;
static atomic_ullong g_total_latency_us = 0;
static atomic_int g_connected = 0;

static volatile int g_running = 1;
static time_t g_start_time = 0;
static int g_duration = 30;
static int g_engine = 0;  // 0=Hash, 1=RBTree, 2=Array
static atomic_ullong g_peak_qps = 0;  // 峰值 QPS

// QPS 采样数据（用于去除极值计算平均）
#define MAX_SAMPLES 300
static unsigned long long g_qps_samples[MAX_SAMPLES];
static int g_sample_count = 0;
static pthread_mutex_t g_sample_lock = PTHREAD_MUTEX_INITIALIZER;

// 线程参数
typedef struct {
    int thread_id;
    const char *host;
    int port_start;
    int port_count;
    int conn_per_thread;
    int rw_ratio;  // 读操作比例 0-100
} ThreadArg;

// 获取当前时间（微秒）
static long long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 创建并连接 socket
static int create_connection(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    // 设置 TCP_NODELAY
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

// 发送请求并接收响应（阻塞模式，优化版）
static int send_request(int fd, const char *cmd, int cmd_len) {
    // 发送（通常一次就够）
    int sent = 0;
    while (sent < cmd_len) {
        int n = send(fd, cmd + sent, cmd_len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    
    // 批量接收响应（等待 \r\n）
    char buf[BUF_SIZE];
    int received = 0;
    while (received < BUF_SIZE - 1) {
        // 批量读取，而非逐字节
        int n = recv(fd, buf + received, BUF_SIZE - 1 - received, 0);
        if (n <= 0) return -1;
        received += n;
        
        // 检查是否收到完整响应（以 \r\n 结尾）
        for (int i = (received - n > 0 ? received - n : 0); i < received - 1; i++) {
            if (buf[i] == '\r' && buf[i+1] == '\n') {
                buf[i+2] = '\0';
                goto check_response;
            }
        }
    }
    
check_response:
    // 检查响应是否成功
    if (buf[0] == 'O' && buf[1] == 'K') {
        return 0;  // OK 开头，成功
    }
    // ERROR: Key not found 也算成功（服务器正确响应了）
    if (received > 10 && strstr(buf, "not found") != NULL) {
        return 0;
    }
    return 1;  // 其他错误
}

// 工作线程
static void *worker_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    int thread_id = targ->thread_id;
    int conn_count = targ->conn_per_thread;
    
    // 分配连接数组
    int *fds = (int *)malloc(sizeof(int) * conn_count);
    if (!fds) {
        fprintf(stderr, "[Thread %d] malloc failed\n", thread_id);
        return NULL;
    }
    
    // 建立连接
    int established = 0;
    for (int i = 0; i < conn_count; i++) {
        int port = targ->port_start + (i % targ->port_count);
        fds[i] = create_connection(targ->host, port);
        if (fds[i] >= 0) {
            established++;
            atomic_fetch_add(&g_connected, 1);
        }
    }
    
    printf("[Thread %d] Established %d/%d connections\n", thread_id, established, conn_count);
    
    if (established == 0) {
        free(fds);
        return NULL;
    }
    
    // 等待所有线程连接完成
    sleep(1);
    
    char cmd[BUF_SIZE];
    int key_count = 1000;  // 预热写入的 key 数量
    
    // ========== 预热阶段：写入数据 ==========
    printf("[Thread %d] Warming up: writing %d keys...\n", thread_id, key_count);
    int warmup_success = 0;
    for (int k = 0; k < key_count && g_running; k++) {
        // 使用第一个连接写入
        int fd = fds[0];
        if (fd < 0) break;
        
        // HSET qps_t{thread_id}_k{k} value_{k}
        int cmd_len = snprintf(cmd, BUF_SIZE, "HSET qps_t%d_k%d value_%d\r\n",
                               thread_id, k, k);
        int ret = send_request(fd, cmd, cmd_len);
        if (ret == 0) warmup_success++;
    }
    printf("[Thread %d] Warmup done: %d/%d keys written\n", thread_id, warmup_success, key_count);
    
    // 等待预热完成
    sleep(1);
    
    // ========== 测试阶段：只读 ==========
    unsigned long long req_id = 0;
    
    while (g_running && (time(NULL) - g_start_time) < g_duration) {
        for (int i = 0; i < conn_count && g_running; i++) {
            if (fds[i] < 0) continue;
            
            // 随机选择一个已存在的 key 进行读取
            int key_idx = rand() % key_count;
            int cmd_len;
            
            // 根据 rw_ratio 决定用 HGET 还是 HEXIST
            if ((rand() % 100) < 50) {
                cmd_len = snprintf(cmd, BUF_SIZE, "HGET qps_t%d_k%d\r\n", thread_id, key_idx);
            } else {
                cmd_len = snprintf(cmd, BUF_SIZE, "HEXIST qps_t%d_k%d\r\n", thread_id, key_idx);
            }
            req_id++;
            
            // 记录发送时间
            long long start_us = get_time_us();
            
            // 发送请求
            int ret = send_request(fds[i], cmd, cmd_len);
            
            // 记录延迟
            long long latency = get_time_us() - start_us;
            atomic_fetch_add(&g_total_latency_us, latency);
            
            // 统计
            atomic_fetch_add(&g_total_requests, 1);
            if (ret == 0) {
                atomic_fetch_add(&g_success_requests, 1);
            } else if (ret > 0) {
                atomic_fetch_add(&g_failed_requests, 1);
            } else {
                // 连接断开，尝试重连
                close(fds[i]);
                int port = targ->port_start + (i % targ->port_count);
                fds[i] = create_connection(targ->host, port);
                atomic_fetch_add(&g_failed_requests, 1);
            }
        }
    }
    
    // 关闭连接
    for (int i = 0; i < conn_count; i++) {
        if (fds[i] >= 0) close(fds[i]);
    }
    free(fds);
    
    return NULL;
}

// 统计线程
static void *stats_thread(void *arg) {
    (void)arg;
    
    unsigned long long last_total = 0;
    
    printf("\n%-8s %12s %12s %12s %12s %12s\n",
           "Time(s)", "Total", "Success", "Failed", "QPS", "Lat(us)");
    printf("------------------------------------------------------------------------\n");
    
    while (g_running && (time(NULL) - g_start_time) < g_duration) {
        sleep(1);
        
        unsigned long long total = atomic_load(&g_total_requests);
        unsigned long long success = atomic_load(&g_success_requests);
        unsigned long long failed = atomic_load(&g_failed_requests);
        unsigned long long latency = atomic_load(&g_total_latency_us);
        
        unsigned long long qps = total - last_total;
        unsigned long long avg_lat = qps > 0 ? (latency / (total > 0 ? total : 1)) : 0;
        
        // 更新峰值 QPS
        unsigned long long current_peak = atomic_load(&g_peak_qps);
        if (qps > current_peak) {
            atomic_store(&g_peak_qps, qps);
        }
        
        // 记录采样（用于计算去除极值的平均）
        pthread_mutex_lock(&g_sample_lock);
        if (g_sample_count < MAX_SAMPLES) {
            g_qps_samples[g_sample_count++] = qps;
        }
        pthread_mutex_unlock(&g_sample_lock);
        
        int elapsed = (int)(time(NULL) - g_start_time);
        printf("%-8d %12llu %12llu %12llu %12llu %12llu\n",
               elapsed, total, success, failed, qps, avg_lat);
        
        last_total = total;
    }
    
    return NULL;
}

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    printf("\nStopping...\n");
}

static int parse_port_range(const char *s, int *start, int *end) {
    int a = 0, b = 0;
    if (sscanf(s, "%d-%d", &a, &b) == 2 && a > 0 && b >= a) {
        *start = a; *end = b; return 0;
    }
    if (sscanf(s, "%d", &a) == 1 && a > 0) {
        *start = a; *end = a; return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    const char *port_str = "2000";
    int connections = 1000;
    int duration = 30;
    int threads = 8;
    int rw_ratio = 50;
    const char *engine_str = "hash";
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0) && i+1 < argc) {
            host = argv[++i];
        } else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i+1 < argc) {
            port_str = argv[++i];
        } else if ((strcmp(argv[i], "--connections") == 0 || strcmp(argv[i], "-c") == 0) && i+1 < argc) {
            connections = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--duration") == 0 || strcmp(argv[i], "-d") == 0) && i+1 < argc) {
            duration = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--threads") == 0 || strcmp(argv[i], "-t") == 0) && i+1 < argc) {
            threads = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--rw-ratio") == 0 || strcmp(argv[i], "-r") == 0) && i+1 < argc) {
            rw_ratio = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--engine") == 0 || strcmp(argv[i], "-e") == 0) && i+1 < argc) {
            engine_str = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("QPS 性能测试客户端\n\n");
            printf("用法: %s [选项]\n\n", argv[0]);
            printf("选项:\n");
            printf("  -h, --host HOST         服务器地址 (默认: 127.0.0.1)\n");
            printf("  -p, --port PORT         端口或端口范围 (默认: 2000)\n");
            printf("  -c, --connections N     总连接数 (默认: 1000)\n");
            printf("  -d, --duration SECS     测试时长秒 (默认: 30)\n");
            printf("  -t, --threads N         工作线程数 (默认: 8)\n");
            printf("  -e, --engine ENGINE     数据结构: hash/rbtree/array (默认: hash)\n");
            printf("      --help              显示帮助\n");
            printf("\n示例:\n");
            printf("  %s -h 192.168.1.100 -p 2000-2019 -c 1000 -e hash\n", argv[0]);
            printf("  %s -h 192.168.1.100 -c 10000 -e rbtree -d 60\n", argv[0]);
            return 0;
        }
    }
    
    // 解析引擎类型
    if (strcmp(engine_str, "hash") == 0 || strcmp(engine_str, "h") == 0) {
        g_engine = 0;
    } else if (strcmp(engine_str, "rbtree") == 0 || strcmp(engine_str, "r") == 0) {
        g_engine = 1;
    } else if (strcmp(engine_str, "array") == 0 || strcmp(engine_str, "a") == 0) {
        g_engine = 2;
    } else {
        fprintf(stderr, "Unknown engine: %s (use hash/rbtree/array)\n", engine_str);
        return 1;
    }
    
    int port_start = 0, port_end = 0;
    if (parse_port_range(port_str, &port_start, &port_end) != 0) {
        fprintf(stderr, "Invalid port: %s\n", port_str);
        return 1;
    }
    int port_count = port_end - port_start + 1;
    
    g_duration = duration;
    
    // 注册信号
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    const char *engine_name = (g_engine == 0) ? "Hash" : (g_engine == 1) ? "RBTree" : "Array";
    
    printf("======================================================\n");
    printf("           QPS 性能测试客户端\n");
    printf("======================================================\n");
    printf("服务器:      %s:%d", host, port_start);
    if (port_count > 1) printf("-%d (%d ports)", port_end, port_count);
    printf("\n");
    printf("数据结构:    %s\n", engine_name);
    printf("连接数:      %d\n", connections);
    printf("线程数:      %d\n", threads);
    printf("测试时长:    %d 秒\n", duration);
    printf("======================================================\n\n");
    
    srand(time(NULL));
    g_start_time = time(NULL);
    
    // 创建工作线程
    pthread_t *worker_tids = malloc(sizeof(pthread_t) * threads);
    ThreadArg *thread_args = malloc(sizeof(ThreadArg) * threads);
    
    int conn_per_thread = connections / threads;
    int extra = connections % threads;
    
    printf("正在建立连接...\n");
    
    for (int i = 0; i < threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].host = host;
        thread_args[i].port_start = port_start;
        thread_args[i].port_count = port_count;
        thread_args[i].conn_per_thread = conn_per_thread + (i < extra ? 1 : 0);
        thread_args[i].rw_ratio = rw_ratio;
        
        pthread_create(&worker_tids[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // 等待连接建立
    sleep(2);
    printf("已建立 %d 个连接\n", atomic_load(&g_connected));
    
    // 启动统计线程
    pthread_t stats_tid;
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    // 等待完成
    for (int i = 0; i < threads; i++) {
        pthread_join(worker_tids[i], NULL);
    }
    
    g_running = 0;
    pthread_join(stats_tid, NULL);
    
    // 最终统计
    unsigned long long total = atomic_load(&g_total_requests);
    unsigned long long success = atomic_load(&g_success_requests);
    unsigned long long failed = atomic_load(&g_failed_requests);
    unsigned long long latency = atomic_load(&g_total_latency_us);
    unsigned long long peak = atomic_load(&g_peak_qps);
    
    // 计算去除极值的平均 QPS
    double avg_qps = 0;
    if (g_sample_count >= 3) {
        // 找出最大和最小值
        unsigned long long min_qps = g_qps_samples[0];
        unsigned long long max_qps = g_qps_samples[0];
        unsigned long long sum = 0;
        for (int i = 0; i < g_sample_count; i++) {
            if (g_qps_samples[i] < min_qps) min_qps = g_qps_samples[i];
            if (g_qps_samples[i] > max_qps) max_qps = g_qps_samples[i];
            sum += g_qps_samples[i];
        }
        // 去除一个最大和一个最小
        sum -= min_qps;
        sum -= max_qps;
        avg_qps = (double)sum / (g_sample_count - 2);
    } else if (g_sample_count > 0) {
        unsigned long long sum = 0;
        for (int i = 0; i < g_sample_count; i++) sum += g_qps_samples[i];
        avg_qps = (double)sum / g_sample_count;
    }
    
    printf("\n======================================================\n");
    printf("                   测试结果\n");
    printf("======================================================\n");
    printf("总请求数:    %llu\n", total);
    printf("成功请求:    %llu\n", success);
    printf("失败请求:    %llu\n", failed);
    printf("成功率:      %.2f%%\n", total > 0 ? (double)success / total * 100 : 0);
    printf("峰值QPS:     %llu\n", peak);
    printf("平均QPS:     %.2f (去除极值)\n", avg_qps);
    printf("平均延迟:    %.2f us\n", total > 0 ? (double)latency / total : 0);
    printf("======================================================\n");
    
    free(worker_tids);
    free(thread_args);
    
    return 0;
}
