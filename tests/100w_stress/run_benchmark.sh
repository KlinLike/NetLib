#!/bin/bash
# ==============================================================================
# 一键性能测试脚本
# ==============================================================================
#
# 功能：
#   1. 自动配置系统参数
#   2. 启动服务器（多端口）
#   3. 启动性能监控
#   4. 执行压测
#   5. 生成报告
#
# 使用方法：
#   ./tests/run_benchmark.sh [选项]
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
START_PORT=2000
PORT_COUNT=10
CONNECTIONS=1000
DURATION=60
RW_RATIO="50:50"
REQUEST_RATE=1
KEEPALIVE="--keepalive"
SKIP_SETUP=false
SKIP_BUILD=false

# PID文件
SERVER_PID=""
MONITOR_PID=""

# 清理函数
cleanup() {
    echo -e "\n${COLOR_YELLOW}正在清理...${COLOR_RESET}"
    
    if [ -n "$MONITOR_PID" ] && kill -0 $MONITOR_PID 2>/dev/null; then
        kill $MONITOR_PID 2>/dev/null || true
        echo "已停止监控进程"
    fi
    
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        kill $SERVER_PID 2>/dev/null || true
        echo "已停止服务器"
        sleep 1
    fi
    
    # 清理所有监听端口
    for i in $(seq 0 $((PORT_COUNT - 1))); do
        local port=$((START_PORT + i))
        local pid=$(lsof -ti:$port 2>/dev/null || true)
        if [ -n "$pid" ]; then
            kill $pid 2>/dev/null || true
        fi
    done
    
    echo -e "${COLOR_GREEN}清理完成${COLOR_RESET}"
}

# 注册清理函数
trap cleanup EXIT INT TERM

# 显示帮助
show_help() {
    cat <<EOF
一键性能测试脚本

用法: $0 [选项]

选项:
    -p, --start-port PORT      起始端口号 (默认: 2000)
    -n, --port-count N         监听端口数量 (默认: 10)
    -c, --connections N        并发连接数 (默认: 10000)
    -d, --duration SECONDS     测试持续时间 (默认: 60)
    -r, --rw-ratio RATIO       读写比例 (默认: 50:50)
    --rate N                   每连接每秒请求数 (默认: 1, 0=最大)
    --short-conn               使用短连接模式
    --skip-setup               跳过系统配置
    --skip-build               跳过编译
    -h, --help                 显示此帮助信息

示例:
    # 默认测试：1万连接，10个端口
    $0

    # 10万连接测试
    $0 -c 100000 -n 20

    # 百万连接测试
    $0 -c 1000000 -n 100 -d 300

    # 只读压测
    $0 -c 50000 -r 100:0 --rate 10

    # 短连接压测
    $0 -c 5000 --short-conn --rate 0

EOF
}

# 解析参数
parse_args() {
    while [ $# -gt 0 ]; do
        case $1 in
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
            -r|--rw-ratio)
                RW_RATIO=$2
                shift 2
                ;;
            --rate)
                REQUEST_RATE=$2
                shift 2
                ;;
            --short-conn)
                KEEPALIVE="--no-keepalive"
                shift
                ;;
            --skip-setup)
                SKIP_SETUP=true
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
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
    echo -e "\n${COLOR_CYAN}================================ 测试配置 ================================${COLOR_RESET}\n"
    echo "服务器配置:"
    echo "  起始端口: $START_PORT"
    echo "  端口数量: $PORT_COUNT (${START_PORT}-$((START_PORT + PORT_COUNT - 1)))"
    echo ""
    echo "客户端配置:"
    echo "  并发连接: $CONNECTIONS"
    echo "  测试时长: ${DURATION}秒"
    echo "  读写比例: $RW_RATIO"
    echo "  请求速率: ${REQUEST_RATE} req/s/conn"
    echo "  连接模式: $([ "$KEEPALIVE" = "--keepalive" ] && echo "长连接" || echo "短连接")"
    echo ""
    echo -e "${COLOR_CYAN}===========================================================================${COLOR_RESET}\n"
}

# 步骤1: 配置系统
setup_system() {
    if [ "$SKIP_SETUP" = true ]; then
        echo -e "${COLOR_YELLOW}跳过系统配置${COLOR_RESET}"
        return
    fi
    
    echo -e "${COLOR_BLUE}[1/5] 配置系统参数...${COLOR_RESET}"
    
    local setup_script="./tests/100w_stress/setup_100w.sh"
    if [ ! -f "$setup_script" ]; then
        echo -e "${COLOR_RED}错误: 找不到 $setup_script${COLOR_RESET}"
        exit 1
    fi
    
    if [ "$EUID" -ne 0 ]; then
        echo -e "${COLOR_YELLOW}需要root权限配置系统，请输入密码：${COLOR_RESET}"
        sudo "$setup_script" auto
    else
        "$setup_script" auto
    fi
    
    echo -e "\n${COLOR_GREEN}✓ 系统配置完成${COLOR_RESET}\n"
}

# 步骤2: 编译服务器
build_server() {
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${COLOR_YELLOW}跳过编译${COLOR_RESET}"
        return
    fi
    
    echo -e "${COLOR_BLUE}[2/5] 编译服务器与客户端...${COLOR_RESET}"
    
    make clean > /dev/null 2>&1 || true
    make -j$(nproc) > /tmp/netlib_make.log 2>&1 || true
    
    if [ ! -f "./build/server" ]; then
        echo -e "${COLOR_RED}错误: 服务器编译失败${COLOR_RESET}"
        tail -n 50 /tmp/netlib_make.log || true
        exit 1
    fi
    
    # 编译10w并发客户端
    gcc -O2 -pthread -o ./build/stress_client ./tests/100w_stress/stress_client.c
    if [ $? -ne 0 ] || [ ! -f "./build/stress_client" ]; then
        echo -e "${COLOR_RED}错误: 客户端编译失败${COLOR_RESET}"
        exit 1
    fi
    
    echo -e "${COLOR_GREEN}✓ 编译完成${COLOR_RESET}\n"
}

# 步骤3: 启动服务器
start_server() {
    echo -e "${COLOR_BLUE}[3/5] 启动服务器...${COLOR_RESET}"
    
    # 检查端口是否被占用
    for i in $(seq 0 $((PORT_COUNT - 1))); do
        local port=$((START_PORT + i))
        if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
            echo -e "${COLOR_RED}错误: 端口 $port 已被占用${COLOR_RESET}"
            exit 1
        fi
    done
    
    # 启动服务器（监听多个端口）
    ./build/server $START_PORT $PORT_COUNT > /tmp/netlib_server.log 2>&1 &
    SERVER_PID=$!
    
    # 等待服务器启动
    echo -n "等待服务器启动"
    for i in {1..10}; do
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
    
    echo -e "${COLOR_GREEN}✓ 服务器已启动 (PID: $SERVER_PID)${COLOR_RESET}"
    echo "  日志文件: /tmp/netlib_server.log"
    echo ""
}

# 步骤4: 启动监控（已移除）
start_monitor() {
    echo -e "${COLOR_BLUE}[4/5] 跳过性能监控${COLOR_RESET}"
}

# 步骤5: 执行压测
run_stress_test() {
    echo -e "${COLOR_BLUE}[5/5] 执行压测...${COLOR_RESET}\n"
    
    local end_port=$((START_PORT + PORT_COUNT - 1))
    
    # 使用C客户端建立10万并发（默认值可被 -c 覆盖）
    local target_conn=$CONNECTIONS
    if [ "$target_conn" -lt 1 ]; then target_conn=100000; fi
    echo -e "运行客户端: 建立并发连接数=$target_conn 端口范围=${START_PORT}-${end_port}"
    # 对于本地低并发测试，减小批次与等待间隔，避免瞬时洪峰
    ./build/stress_client --host 127.0.0.1 --port-range ${START_PORT}-${end_port} --connections $target_conn --batch 500 --interval-ms 50
}

# 主函数
main() {
    parse_args "$@"
    
    echo -e "${COLOR_CYAN}"
    cat << "EOF"
    _   __     __  __    _ __  
   / | / /__  / /_/ /   (_) /_ 
  /  |/ / _ \/ __/ /   / / __ \
 / /|  /  __/ /_/ /___/ / /_/ /
/_/ |_/\___/\__/_____/_/_.___/ 
                                
百万级并发压测系统
EOF
    echo -e "${COLOR_RESET}"
    
    print_config
    
    # 询问是否继续
    if [ "$CONNECTIONS" -ge 100000 ]; then
        echo -e "${COLOR_YELLOW}⚠ 警告: 即将进行大规模压测 ($CONNECTIONS 连接)${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}⚠ 这可能消耗大量系统资源${COLOR_RESET}"
        read -p "是否继续? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "测试已取消"
            exit 0
        fi
    fi
    
    # 执行测试流程
    setup_system
    build_server
    start_server
    start_monitor
    run_stress_test
    
    echo -e "\n${COLOR_GREEN}✓ 测试完成${COLOR_RESET}\n"
}

# 检查依赖
check_dependencies() {
    local missing=()
    
    for cmd in make gcc lsof ss; do
        if ! command -v $cmd &> /dev/null; then
            missing+=($cmd)
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${COLOR_RED}错误: 缺少必要的命令: ${missing[*]}${COLOR_RESET}"
        exit 1
    fi
}

# 程序入口
check_dependencies
main "$@"
