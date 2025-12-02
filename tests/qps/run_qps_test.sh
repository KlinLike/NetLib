#!/bin/bash
# ==============================================================================
# QPS 性能测试脚本
# ==============================================================================
#
# 功能：测试服务器每秒处理请求数（QPS）
#
# 使用方法：
#   ./tests/qps/run_qps_test.sh [选项]
#
# ==============================================================================

set -e

# 颜色定义
COLOR_GREEN="\033[0;32m"
COLOR_YELLOW="\033[1;33m"
COLOR_RED="\033[0;31m"
COLOR_BLUE="\033[0;34m"
COLOR_CYAN="\033[0;36m"
COLOR_RESET="\033[0m"

# 默认参数
HOST="127.0.0.1"
START_PORT=2000
PORT_COUNT=20
CONNECTIONS=1000
DURATION=30
THREADS=4
RW_RATIO=50
SKIP_BUILD=false
SKIP_SERVER=false

# PID文件
SERVER_PID=""

# 清理函数
cleanup() {
    echo -e "\n${COLOR_YELLOW}正在清理...${COLOR_RESET}"
    
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        kill $SERVER_PID 2>/dev/null || true
        echo "已停止服务器"
        sleep 1
    fi
    
    echo -e "${COLOR_GREEN}清理完成${COLOR_RESET}"
}

# 注册清理函数
trap cleanup EXIT INT TERM

# 显示帮助
show_help() {
    cat <<EOF
QPS 性能测试脚本

用法: $0 [选项]

选项:
    --host HOST            服务器地址 (默认: 127.0.0.1)
    -p, --start-port PORT  起始端口号 (默认: 2000)
    -n, --port-count N     监听端口数量 (默认: 20)
    -c, --connections N    并发连接数 (默认: 1000)
    -d, --duration SECS    测试持续时间 (默认: 30)
    -t, --threads N        工作线程数 (默认: 4)
    -r, --rw-ratio N       读操作百分比 (默认: 50)
    --skip-build           跳过编译
    --skip-server          跳过启动服务器（用于远程测试）
    -h, --help             显示此帮助信息

示例:
    # 本地测试，1000连接
    $0

    # 本地高并发测试
    $0 -c 10000 -t 8 -d 60

    # 远程服务器测试
    $0 --host 192.168.1.100 --skip-server -c 5000

    # 只读压测
    $0 -c 5000 -r 100

    # 只写压测
    $0 -c 5000 -r 0

EOF
}

# 解析参数
parse_args() {
    while [ $# -gt 0 ]; do
        case $1 in
            --host)
                HOST=$2
                shift 2
                ;;
            -p|--start-port)
                START_PORT=$2
                shift 2
                ;;
            -n|--port-count)
                PORT_COUNT=$2
                shift 2
                ;;
            -c|--connections)
                CONNECTIONS=$2
                shift 2
                ;;
            -d|--duration)
                DURATION=$2
                shift 2
                ;;
            -t|--threads)
                THREADS=$2
                shift 2
                ;;
            -r|--rw-ratio)
                RW_RATIO=$2
                shift 2
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --skip-server)
                SKIP_SERVER=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo -e "${COLOR_RED}未知参数: $1${COLOR_RESET}"
                show_help
                exit 1
                ;;
        esac
    done
}

# 打印配置
print_config() {
    local end_port=$((START_PORT + PORT_COUNT - 1))
    echo -e "\n${COLOR_CYAN}================================ QPS测试配置 ================================${COLOR_RESET}\n"
    echo "服务器配置:"
    echo "  目标地址: $HOST"
    echo "  端口范围: ${START_PORT}-${end_port} ($PORT_COUNT 个端口)"
    echo ""
    echo "测试配置:"
    echo "  并发连接: $CONNECTIONS"
    echo "  工作线程: $THREADS"
    echo "  测试时长: ${DURATION}秒"
    echo "  读写比例: ${RW_RATIO}:$((100 - RW_RATIO)) (读:写)"
    echo ""
    echo -e "${COLOR_CYAN}===========================================================================${COLOR_RESET}\n"
}

# 编译
build() {
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${COLOR_YELLOW}跳过编译${COLOR_RESET}"
        return
    fi
    
    echo -e "${COLOR_BLUE}[1/3] 编译服务器与客户端...${COLOR_RESET}"
    
    # 编译服务器
    if [ "$SKIP_SERVER" = false ]; then
        make -j$(nproc) > /tmp/netlib_make.log 2>&1 || {
            echo -e "${COLOR_RED}错误: 服务器编译失败${COLOR_RESET}"
            tail -n 20 /tmp/netlib_make.log
            exit 1
        }
    fi
    
    # 编译QPS客户端
    gcc -O2 -pthread -o ./build/qps_client ./tests/qps/qps_client.c 2>&1 || {
        echo -e "${COLOR_RED}错误: QPS客户端编译失败${COLOR_RESET}"
        exit 1
    }
    
    echo -e "${COLOR_GREEN}✓ 编译完成${COLOR_RESET}\n"
}

# 启动服务器
start_server() {
    if [ "$SKIP_SERVER" = true ]; then
        echo -e "${COLOR_YELLOW}跳过启动服务器（远程测试模式）${COLOR_RESET}"
        return
    fi
    
    echo -e "${COLOR_BLUE}[2/3] 启动服务器...${COLOR_RESET}"
    
    # 检查端口是否被占用
    for i in $(seq 0 $((PORT_COUNT - 1))); do
        local port=$((START_PORT + i))
        if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
            echo -e "${COLOR_YELLOW}警告: 端口 $port 已被占用，尝试停止...${COLOR_RESET}"
            local pid=$(lsof -ti:$port 2>/dev/null || true)
            if [ -n "$pid" ]; then
                kill $pid 2>/dev/null || true
                sleep 0.5
            fi
        fi
    done
    
    # 启动服务器
    ./build/server $START_PORT $PORT_COUNT > /tmp/netlib_server.log 2>&1 &
    SERVER_PID=$!
    
    # 等待服务器启动
    echo -n "等待服务器启动"
    for i in {1..20}; do
        sleep 0.5
        echo -n "."
        if lsof -Pi :$START_PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
            break
        fi
    done
    echo ""
    
    # 验证服务器是否启动成功
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${COLOR_RED}错误: 服务器启动失败${COLOR_RESET}"
        cat /tmp/netlib_server.log
        exit 1
    fi
    
    echo -e "${COLOR_GREEN}✓ 服务器已启动 (PID: $SERVER_PID)${COLOR_RESET}\n"
}

# 执行QPS测试
run_qps_test() {
    echo -e "${COLOR_BLUE}[3/3] 执行QPS测试...${COLOR_RESET}\n"
    
    local end_port=$((START_PORT + PORT_COUNT - 1))
    
    ./build/qps_client \
        --host $HOST \
        --port-range ${START_PORT}-${end_port} \
        --connections $CONNECTIONS \
        --duration $DURATION \
        --threads $THREADS \
        --rw-ratio $RW_RATIO
}

# 主函数
main() {
    parse_args "$@"
    
    echo -e "${COLOR_CYAN}"
    cat << "EOF"
   ____  ____  _____   _____         _   
  / __ \|  _ \/ ____| |_   _|__  ___| |_ 
 | |  | | |_) | (___     | |/ _ \/ __| __|
 | |  | |  __/ \___ \    | |  __/\__ \ |_ 
 |  \/  | |    ____) |   | |\___||___/\__|
  \___\_\_|   |_____/    |_|              
                                          
EOF
    echo -e "${COLOR_RESET}"
    
    print_config
    
    build
    start_server
    run_qps_test
    
    echo -e "\n${COLOR_GREEN}✓ 测试完成${COLOR_RESET}\n"
}

# 程序入口
main "$@"
