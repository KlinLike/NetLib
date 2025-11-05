#!/bin/bash

# KVS 协议统一测试脚本
# 用法: ./test_protocol.sh

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  KVS 协议统一测试${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# 1. 清理旧文件
echo -e "${YELLOW}[1/3] 清理旧文件...${NC}"
if [ -f "test_protocol" ]; then
    rm -f test_protocol
    echo "  ✓ 删除旧的可执行文件"
fi
echo ""

# 2. 编译
echo -e "${YELLOW}[2/3] 编译程序...${NC}"
gcc -o test_protocol \
    tests/test_protocol.c \
    src/kvs_base.c \
    src/kvs_array.c \
    src/kvs_rbtree.c \
    src/kvs_hash.c \
    src/kvs_protocol.c \
    -I./include \
    -Wall -Wextra \
    -pthread \
    -g

if [ $? -eq 0 ]; then
    echo -e "  ${GREEN}✓ 编译成功${NC}"
else
    echo -e "  ${RED}✗ 编译失败${NC}"
    exit 1
fi
echo ""

# 3. 运行测试
echo -e "${YELLOW}[3/3] 运行测试...${NC}"
echo -e "${BLUE}----------------------------------------${NC}"

./test_protocol
TEST_RESULT=$?

echo -e "${BLUE}----------------------------------------${NC}"
echo ""

# 4. 清理
if [ $TEST_RESULT -eq 0 ]; then
    if [ -f "test_protocol" ]; then
        rm -f test_protocol
        echo -e "${GREEN}✓ 测试成功，已清理可执行文件${NC}"
    fi
    exit 0
else
    echo -e "${RED}✗ 测试失败${NC}"
    echo -e "${YELLOW}提示: 保留编译文件 test_protocol 以便调试${NC}"
    exit 1
fi

