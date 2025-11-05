#include "../include/kvstore.h"
#include "../include/kvs_protocol.h"
#include "../include/kvs_rbtree.h"
#include "../include/kvs_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== 颜色定义 ==========
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_RESET   "\033[0m"

// ========== 测试统计 ==========
typedef struct {
    int total;
    int passed;
    int failed;
} test_stats_t;

test_stats_t g_stats = {0, 0, 0};

// ========== 工具函数 ==========
void print_separator(const char* title) {
    printf("\n");
    printf(COLOR_CYAN "========================================\n");
    if (title) {
        printf("  %s\n", title);
        printf("========================================\n" COLOR_RESET);
    }
}

void print_test_header(const char* test_name) {
    printf("\n" COLOR_BLUE "【测试】%s" COLOR_RESET "\n", test_name);
    printf("------------------------------------------\n");
}

void print_result(const char* test, int passed) {
    g_stats.total++;
    if(passed) {
        g_stats.passed++;
        printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test);
    } else {
        g_stats.failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test);
    }
}

// ========== 协议基础测试（不依赖特定数据结构）==========

void test_tokenizer() {
    print_test_header("分词器测试 (kvs_tokenizer)");
    
    // 测试1: SET命令
    char msg1[] = "SET name Alice";
    char* tokens1[10] = {NULL};
    int count1 = kvs_tokenizer(msg1, tokens1);
    printf("输入: \"%s\"\n", "SET name Alice");
    printf("输出: [%s] [%s] [%s], 总数=%d\n", tokens1[0], tokens1[1], tokens1[2], count1);
    print_result("SET命令分词", count1 == 3 && strcmp(tokens1[0], "SET") == 0);
    
    // 测试2: GET命令
    char msg2[] = "GET name";
    char* tokens2[10] = {NULL};
    int count2 = kvs_tokenizer(msg2, tokens2);
    printf("输入: \"%s\"\n", "GET name");
    printf("输出: [%s] [%s], 总数=%d\n", tokens2[0], tokens2[1], count2);
    print_result("GET命令分词", count2 == 2 && strcmp(tokens2[0], "GET") == 0);
    
    // 测试3: 带下划线的值
    char msg3[] = "SET key value_with_underscore";
    char* tokens3[10] = {NULL};
    int count3 = kvs_tokenizer(msg3, tokens3);
    printf("输入: \"%s\"\n", "SET key value_with_underscore");
    printf("输出: [%s] [%s] [%s], 总数=%d\n", tokens3[0], tokens3[1], tokens3[2], count3);
    print_result("带下划线的值分词", count3 == 3);
}

void test_parser() {
    print_test_header("命令识别器测试 (kvs_parser_command)");
    
    // 准备测试数据
    struct {
        const char* cmd_str;
        int expected_cmd;
    } tests[] = {
        {"SET", KVS_CMD_SET},
        {"GET", KVS_CMD_GET},
        {"DEL", KVS_CMD_DEL},
        {"MOD", KVS_CMD_MOD},
        {"EXIST", KVS_CMD_EXIST},
        {"RSET", KVS_CMD_RSET},
        {"RGET", KVS_CMD_RGET},
        {"RMOD", KVS_CMD_RMOD},
        {"RDEL", KVS_CMD_RDEL},
        {"REXIST", KVS_CMD_REXIST},
        {"HSET", KVS_CMD_HSET},
        {"HGET", KVS_CMD_HGET},
        {"HMOD", KVS_CMD_HMOD},
        {"HDEL", KVS_CMD_HDEL},
        {"HEXIST", KVS_CMD_HEXIST},
    };
    
    int num_tests = sizeof(tests)/sizeof(tests[0]);
    for(int i = 0; i < num_tests; i++) {
        char msg[100];
        strcpy(msg, tests[i].cmd_str);
        strcat(msg, " key value");
        
        char* tokens[10] = {NULL};
        kvs_tokenizer(msg, tokens);
        
        int cmd = kvs_parser_command(tokens);
        int passed = (cmd == tests[i].expected_cmd);
        if (!passed) {
            printf("命令: %s -> 识别为: %d (预期: %d) ", 
                   tests[i].cmd_str, cmd, tests[i].expected_cmd);
        }
        print_result(tests[i].cmd_str, passed);
    }
    
    // 测试无效命令
    char invalid_msg[] = "INVALID key value";
    char* invalid_tokens[10] = {NULL};
    kvs_tokenizer(invalid_msg, invalid_tokens);
    int invalid_cmd = kvs_parser_command(invalid_tokens);
    print_result("无效命令识别（应返回负数）", invalid_cmd < 0);
}

// ========== Array协议测试 ==========

void test_array_protocol() {
    print_test_header("Array协议集成测试");
    
    // 初始化全局数组
    global_array = (kvs_array_t*)kvs_malloc(sizeof(kvs_array_t));
    global_array->table = NULL;
    if (kvs_array_create(global_array) != KVS_OK) {
        printf(COLOR_RED "✗ 初始化Array失败\n" COLOR_RESET);
        return;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 初始化Array成功\n\n");
    
    char response[1024];
    char* tokens[10];
    
    // 测试SET命令
    char msg1[] = "SET name Alice";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg1, tokens);
    int cmd1 = kvs_parser_command(tokens);
    kvs_executor_command(cmd1, tokens, response);
    print_result("SET name Alice", strcmp(response, "OK") == 0);
    
    // 测试GET命令
    char msg2[] = "GET name";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg2, tokens);
    int cmd2 = kvs_parser_command(tokens);
    kvs_executor_command(cmd2, tokens, response);
    print_result("GET name (Alice)", strstr(response, "Alice") != NULL);
    
    // 测试MOD命令
    char msg3[] = "MOD name Bob";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg3, tokens);
    int cmd3 = kvs_parser_command(tokens);
    kvs_executor_command(cmd3, tokens, response);
    print_result("MOD name Bob", strcmp(response, "OK") == 0);
    
    // 验证MOD
    char msg4[] = "GET name";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg4, tokens);
    int cmd4 = kvs_parser_command(tokens);
    kvs_executor_command(cmd4, tokens, response);
    print_result("验证MOD (Bob)", strstr(response, "Bob") != NULL);
    
    // 测试EXIST命令
    char msg5[] = "EXIST name";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg5, tokens);
    int cmd5 = kvs_parser_command(tokens);
    kvs_executor_command(cmd5, tokens, response);
    print_result("EXIST name", strcmp(response, "OK") == 0);
    
    // 测试DEL命令
    char msg6[] = "DEL name";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg6, tokens);
    int cmd6 = kvs_parser_command(tokens);
    kvs_executor_command(cmd6, tokens, response);
    print_result("DEL name", strcmp(response, "OK") == 0);
    
    // 验证DEL
    char msg7[] = "GET name";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg7, tokens);
    int cmd7 = kvs_parser_command(tokens);
    kvs_executor_command(cmd7, tokens, response);
    print_result("验证DEL (not found)", strstr(response, "not found") != NULL);
    
    // 清理
    kvs_array_destroy(global_array);
    kvs_free(global_array);
}

// ========== RBTree协议测试 ==========

void test_rbtree_protocol() {
    print_test_header("RBTree协议集成测试");
    
    // 初始化红黑树
    if (kvs_rbtree_create(global_rbtree) != KVS_OK) {
        printf(COLOR_RED "✗ 初始化RBTree失败\n" COLOR_RESET);
        return;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 初始化RBTree成功\n\n");
    
    char response[1024];
    char* tokens[10];
    
    // 测试RSET命令
    char msg1[] = "RSET name Alice";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg1, tokens);
    int cmd1 = kvs_parser_command(tokens);
    kvs_executor_command(cmd1, tokens, response);
    print_result("RSET name Alice", strcmp(response, "OK") == 0);
    
    // 测试RGET命令
    char msg2[] = "RGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg2, tokens);
    int cmd2 = kvs_parser_command(tokens);
    kvs_executor_command(cmd2, tokens, response);
    print_result("RGET name (Alice)", strstr(response, "Alice") != NULL);
    
    // 测试RMOD命令
    char msg3[] = "RMOD name Bob";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg3, tokens);
    int cmd3 = kvs_parser_command(tokens);
    kvs_executor_command(cmd3, tokens, response);
    print_result("RMOD name Bob", strcmp(response, "OK") == 0);
    
    // 验证RMOD
    char msg4[] = "RGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg4, tokens);
    int cmd4 = kvs_parser_command(tokens);
    kvs_executor_command(cmd4, tokens, response);
    print_result("验证RMOD (Bob)", strstr(response, "Bob") != NULL);
    
    // 测试REXIST命令
    char msg5[] = "REXIST name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg5, tokens);
    int cmd5 = kvs_parser_command(tokens);
    kvs_executor_command(cmd5, tokens, response);
    print_result("REXIST name", strcmp(response, "OK") == 0);
    
    // 测试RDEL命令
    char msg6[] = "RDEL name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg6, tokens);
    int cmd6 = kvs_parser_command(tokens);
    kvs_executor_command(cmd6, tokens, response);
    print_result("RDEL name", strcmp(response, "OK") == 0);
    
    // 验证RDEL
    char msg7[] = "RGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg7, tokens);
    int cmd7 = kvs_parser_command(tokens);
    kvs_executor_command(cmd7, tokens, response);
    print_result("验证RDEL (not found)", strstr(response, "not found") != NULL);
    
    // 批量测试
    printf("\n批量插入100个键值对...\n");
    int success = 0;
    for(int i = 0; i < 100; i++){
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "RSET key_%d value_%d", i, i);
        
        char msg[256];
        strcpy(msg, cmd);
        char* test_tokens[10] = {0};
        kvs_tokenizer(msg, test_tokens);
        int cmd_id = kvs_parser_command(test_tokens);
        char test_response[512];
        kvs_executor_command(cmd_id, test_tokens, test_response);
        
        if(strncmp(test_response, "OK", 2) == 0){
            success++;
        }
    }
    print_result("批量插入100个键值对", success == 100);
    
    // 清理
    kvs_rbtree_destroy(global_rbtree);
}

// ========== Hash协议测试 ==========

void test_hash_protocol() {
    print_test_header("Hash协议集成测试");
    
    // 初始化哈希表
    if (kvs_hash_create(global_hash) != KVS_OK) {
        printf(COLOR_RED "✗ 初始化Hash失败\n" COLOR_RESET);
        return;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 初始化Hash成功\n\n");
    
    char response[1024];
    char* tokens[10];
    
    // 测试HSET命令
    char msg1[] = "HSET name Alice";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg1, tokens);
    int cmd1 = kvs_parser_command(tokens);
    kvs_executor_command(cmd1, tokens, response);
    print_result("HSET name Alice", strcmp(response, "OK") == 0);
    
    // 测试HGET命令
    char msg2[] = "HGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg2, tokens);
    int cmd2 = kvs_parser_command(tokens);
    kvs_executor_command(cmd2, tokens, response);
    print_result("HGET name (Alice)", strstr(response, "Alice") != NULL);
    
    // 测试HMOD命令
    char msg3[] = "HMOD name Bob";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg3, tokens);
    int cmd3 = kvs_parser_command(tokens);
    kvs_executor_command(cmd3, tokens, response);
    print_result("HMOD name Bob", strcmp(response, "OK") == 0);
    
    // 验证HMOD
    char msg4[] = "HGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg4, tokens);
    int cmd4 = kvs_parser_command(tokens);
    kvs_executor_command(cmd4, tokens, response);
    print_result("验证HMOD (Bob)", strstr(response, "Bob") != NULL);
    
    // 测试HEXIST命令
    char msg5[] = "HEXIST name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg5, tokens);
    int cmd5 = kvs_parser_command(tokens);
    kvs_executor_command(cmd5, tokens, response);
    print_result("HEXIST name", strcmp(response, "OK") == 0);
    
    // 测试HDEL命令
    char msg6[] = "HDEL name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg6, tokens);
    int cmd6 = kvs_parser_command(tokens);
    kvs_executor_command(cmd6, tokens, response);
    print_result("HDEL name", strcmp(response, "OK") == 0);
    
    // 验证HDEL
    char msg7[] = "HGET name dummy";
    memset(tokens, 0, sizeof(tokens));
    kvs_tokenizer(msg7, tokens);
    int cmd7 = kvs_parser_command(tokens);
    kvs_executor_command(cmd7, tokens, response);
    print_result("验证HDEL (not found)", strstr(response, "not found") != NULL);
    
    // 批量测试
    printf("\n批量插入100个键值对...\n");
    int success = 0;
    for(int i = 0; i < 100; i++){
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "HSET key_%d value_%d", i, i);
        
        char msg[256];
        strcpy(msg, cmd);
        char* test_tokens[10] = {0};
        kvs_tokenizer(msg, test_tokens);
        int cmd_id = kvs_parser_command(test_tokens);
        char test_response[512];
        kvs_executor_command(cmd_id, test_tokens, test_response);
        
        if(strncmp(test_response, "OK", 2) == 0){
            success++;
        }
    }
    print_result("批量插入100个键值对", success == 100);
    
    // 清理
    kvs_hash_destroy(global_hash);
}

// ========== 主函数 ==========

int main() {
    print_separator("KVS 协议统一测试");
    printf("\n");
    printf(COLOR_CYAN "  本测试将验证:\n");
    printf("  • 协议解析器（分词、命令识别）\n");
    printf("  • Array协议集成\n");
    printf("  • RBTree协议集成\n");
    printf("  • Hash协议集成\n" COLOR_RESET);
    
    // 第一部分：协议基础测试
    print_separator("第一部分：协议基础功能");
    test_tokenizer();
    test_parser();
    
    // 第二部分：各数据结构协议集成测试
    print_separator("第二部分：协议集成测试");
    test_array_protocol();
    test_rbtree_protocol();
    test_hash_protocol();
    
    // 输出测试总结
    print_separator("测试总结");
    printf("\n");
    printf("  总测试数: %d\n", g_stats.total);
    printf(COLOR_GREEN "  通过: %d\n" COLOR_RESET, g_stats.passed);
    if (g_stats.failed > 0) {
        printf(COLOR_RED "  失败: %d\n" COLOR_RESET, g_stats.failed);
    }
    
    double pass_rate = (double)g_stats.passed / g_stats.total * 100;
    printf("\n  通过率: %.1f%%\n", pass_rate);
    
    if (g_stats.failed == 0) {
        printf("\n" COLOR_GREEN "🎉 所有测试通过！\n" COLOR_RESET);
        print_separator(NULL);
        return 0;
    } else {
        printf("\n" COLOR_RED "❌ 有测试失败\n" COLOR_RESET);
        print_separator(NULL);
        return 1;
    }
}

