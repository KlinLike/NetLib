# ==============================================================================
# NetLib Makefile
# ==============================================================================
#
# 编译选项：
#   make          - 开发模式（-g 调试信息，INFO 日志级别）
#   make release  - 发布模式（-O2 优化，只输出 ERROR 日志，最高性能）
#   make log      - 调试模式（输出所有 DEBUG 日志）
#   make clean    - 清理所有编译产物
#
# 测试工具：
#   make qps      - 编译 QPS 性能测试客户端
#   make c1000k   - 编译 C1000K 压力测试客户端
#
# 日志级别（可通过 LOG_LEVEL=x 覆盖）：
#   0 = DEBUG   显示所有日志
#   1 = INFO    隐藏 DEBUG（默认）
#   2 = WARN    只显示 WARN 和 ERROR
#   3 = ERROR   只显示 ERROR（release 默认）
#
# 运行服务器：
#   ./build/server <起始端口> <端口数量>
#   例如: ./build/server 2000 20    # 监听 2000-2019
#
# ==============================================================================

CC = gcc
CFLAGS_BASE = -Wall -Iinclude
LDFLAGS = -pthread

# 目录定义
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TARGET = $(BUILD_DIR)/server

# 源文件和目标文件
SRCS = \
    $(SRC_DIR)/reactor.c \
    $(SRC_DIR)/kvstore.c \
    $(SRC_DIR)/dispatcher.c \
    $(SRC_DIR)/http.c \
    $(SRC_DIR)/echo.c \
    $(SRC_DIR)/kvs_protocol.c \
    $(SRC_DIR)/kvs_base.c \
    $(SRC_DIR)/kvs_array.c \
    $(SRC_DIR)/kvs_rbtree.c \
    $(SRC_DIR)/kvs_hash.c
OBJS = \
    $(BUILD_DIR)/reactor.o \
    $(BUILD_DIR)/kvstore.o \
    $(BUILD_DIR)/dispatcher.o \
    $(BUILD_DIR)/http.o \
    $(BUILD_DIR)/echo.o \
    $(BUILD_DIR)/kvs_protocol.o \
    $(BUILD_DIR)/kvs_base.o \
    $(BUILD_DIR)/kvs_array.o \
    $(BUILD_DIR)/kvs_rbtree.o \
    $(BUILD_DIR)/kvs_hash.o

# 编译选项：设置日志级别
# -DLOG_LEVEL=0  # DEBUG (显示所有日志)
# -DLOG_LEVEL=1  # INFO  (隐藏DEBUG日志)
# -DLOG_LEVEL=2  # WARN  (只显示WARNING和ERROR)
# -DLOG_LEVEL=3  # ERROR (只显示ERROR)

# 默认日志级别（可被目标覆盖）
LOG_LEVEL ?= 1
OPT_FLAGS ?= -g

# 最终 CFLAGS
CFLAGS = $(CFLAGS_BASE) $(OPT_FLAGS) -DLOG_LEVEL=$(LOG_LEVEL)

# 默认编译
all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ 编译完成: $(TARGET)"

$(BUILD_DIR)/reactor.o: $(SRC_DIR)/reactor.c $(INC_DIR)/server.h $(INC_DIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvstore.o: $(SRC_DIR)/kvstore.c $(INC_DIR)/kvstore.h $(INC_DIR)/server.h $(INC_DIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dispatcher.o: $(SRC_DIR)/dispatcher.c $(INC_DIR)/server.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/http.o: $(SRC_DIR)/http.c $(INC_DIR)/server.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/echo.o: $(SRC_DIR)/echo.c $(INC_DIR)/server.h $(INC_DIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvs_protocol.o: $(SRC_DIR)/kvs_protocol.c $(INC_DIR)/kvstore.h $(INC_DIR)/kvs_protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvs_base.o: $(SRC_DIR)/kvs_base.c $(INC_DIR)/kvstore.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvs_array.o: $(SRC_DIR)/kvs_array.c $(INC_DIR)/kvstore.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvs_rbtree.o: $(SRC_DIR)/kvs_rbtree.c $(INC_DIR)/kvstore.h $(INC_DIR)/kvs_rbtree.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kvs_hash.o: $(SRC_DIR)/kvs_hash.c $(INC_DIR)/kvstore.h $(INC_DIR)/kvs_hash.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ 清理完成"

# 发布版本（只输出ERROR日志，最高性能）
release: clean
	$(MAKE) LOG_LEVEL=3 OPT_FLAGS=-O2 all

run: all
	$(TARGET) 2000

# 仅测试 Reactor：编译并运行一个最小化的 echo handler
reactor-test: CFLAGS += -DLOG_LEVEL=0
reactor-test: $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude tests/test_reactor.c src/reactor.c -o $(BUILD_DIR)/reactor_test $(LDFLAGS)
	@echo "✓ 编译完成: $(BUILD_DIR)/reactor_test"

# 编译 C1000K 压力测试客户端
c1000k: $(BUILD_DIR)
	$(CC) -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200112L tests/c1000k/stress_client.c -o $(BUILD_DIR)/stress_client
	@echo "✓ 编译完成: $(BUILD_DIR)/stress_client"

# 编译 QPS 测试客户端
qps: $(BUILD_DIR)
	$(CC) -O2 -Wall -Wextra -pthread tests/qps/qps_client.c -o $(BUILD_DIR)/qps_client
	@echo "✓ 编译完成: $(BUILD_DIR)/qps_client"

# 编译带全量日志（DEBUG）
log: clean
	$(MAKE) LOG_LEVEL=0 all

.PHONY: reactor-test log

.PHONY: all clean release c1000k qps
