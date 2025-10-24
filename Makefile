# ==============================================================================
# NetLib Makefile
# ==============================================================================
#
# 使用说明：
#   make          - 编译项目（开发模式，包含调试信息和所有日志）
#   make release  - 编译发布版本（O2优化，隐藏DEBUG日志）
#   make clean    - 清理所有编译产物
#
# 运行服务器：
#   ./build/echo_server <端口号>
#   例如: ./build/echo_server 8888
#
# ==============================================================================

CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = 

# 目录定义
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TARGET = $(BUILD_DIR)/echo_server

# 源文件和目标文件
SRCS = $(SRC_DIR)/reactor.c $(SRC_DIR)/kvstore.c $(SRC_DIR)/echo.c
OBJS = $(BUILD_DIR)/reactor.o $(BUILD_DIR)/kvstore.o $(BUILD_DIR)/echo.o

# 编译选项：设置日志级别
# -DLOG_LEVEL=0  # DEBUG (显示所有日志)
# -DLOG_LEVEL=1  # INFO  (隐藏DEBUG日志)
# -DLOG_LEVEL=2  # WARN  (只显示WARNING和ERROR)
# -DLOG_LEVEL=3  # ERROR (只显示ERROR)

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

$(BUILD_DIR)/echo.o: $(SRC_DIR)/echo.c $(INC_DIR)/server.h $(INC_DIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ 清理完成"

# 发布版本（隐藏DEBUG日志）
release: CFLAGS += -DLOG_LEVEL=1 -O2
release: clean all

.PHONY: all clean release

