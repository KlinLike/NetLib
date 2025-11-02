#!/bin/bash

# KVS Array 一键编译测试脚本
# 用法: ./test_kvs.sh

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  KVS Array 编译测试脚本${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 1. 清理旧文件
echo -e "${YELLOW}[1/4] 清理旧文件...${NC}"
if [ -f "test_kvs_array" ]; then
    rm -f test_kvs_array
    echo "  ✓ 删除旧的可执行文件"
fi
echo ""

# 2. 编译
echo -e "${YELLOW}[2/4] 编译程序...${NC}"
gcc -o test_kvs_array \
    tests/test_kvs_array.c \
    src/kvs_array.c \
    -I./include \
    -Wall -Wextra \
    -g

if [ $? -eq 0 ]; then
    echo -e "  ${GREEN}✓ 编译成功${NC}"
else
    echo -e "  ${RED}✗ 编译失败${NC}"
    exit 1
fi
echo ""

# 3. 运行测试
echo -e "${YELLOW}[3/4] 运行测试...${NC}"
echo -e "${BLUE}----------------------------------------${NC}"
./test_kvs_array
TEST_RESULT=$?
echo -e "${BLUE}----------------------------------------${NC}"
echo ""

# 4. 检查测试结果
echo -e "${YELLOW}[4/4] 检查结果...${NC}"
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "  ${GREEN}✓ 所有测试通过！${NC}"
    echo ""
    
    # 清理编译产生的文件
    echo -e "${YELLOW}[5/5] 清理编译文件...${NC}"
    if [ -f "test_kvs_array" ]; then
        rm -f test_kvs_array
        echo -e "  ${GREEN}✓ 已删除 test_kvs_array${NC}"
    fi
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  测试成功完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    exit 0
else
    echo -e "  ${RED}✗ 测试失败，退出码: $TEST_RESULT${NC}"
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}  测试失败！${NC}"
    echo -e "${RED}========================================${NC}"
    echo -e "${YELLOW}提示: 保留编译文件 test_kvs_array 以便调试${NC}"
    exit 1
fi

