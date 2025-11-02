#include "../include/kvstore.h"
#include "../include/kvs_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 基础工具函数（kvs_free, kvs_malloc, kvs_strerror, global_array）
// 已在 kvs_base.c 中定义，通过链接使用

// 测试辅助函数
void print_test_header(const char* test_name) {
    printf("\n【测试】%s\n", test_name);
    printf("==========================================\n");
}

void print_result(const char* test, int passed) {
    if(passed) {
        printf("✓ %s - 通过\n", test);
    } else {
        printf("✗ %s - 失败\n", test);
    }
}

// 测试分词器
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
    printf("\n输入: \"%s\"\n", "GET name");
    printf("输出: [%s] [%s], 总数=%d\n", tokens2[0], tokens2[1], count2);
    print_result("GET命令分词", count2 == 2 && strcmp(tokens2[0], "GET") == 0);
    
    // 测试3: 带空格的值
    char msg3[] = "SET key value_with_underscore";
    char* tokens3[10] = {NULL};
    int count3 = kvs_tokenizer(msg3, tokens3);
    printf("\n输入: \"%s\"\n", "SET key value_with_underscore");
    printf("输出: [%s] [%s] [%s], 总数=%d\n", tokens3[0], tokens3[1], tokens3[2], count3);
    print_result("带下划线的值分词", count3 == 3);
}

// 测试命令识别器
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
        {"HGET", KVS_CMD_HGET},
    };
    
    int num_tests = sizeof(tests)/sizeof(tests[0]);
    for(int i = 0; i < num_tests; i++) {
        char msg[100];
        strcpy(msg, tests[i].cmd_str);
        strcat(msg, " key value");
        
        char* tokens[10] = {NULL};
        kvs_tokenizer(msg, tokens);
        
        int cmd = kvs_parser_command(tokens);
        printf("命令: %s -> 识别为: %d (预期: %d)\n", 
               tests[i].cmd_str, cmd, tests[i].expected_cmd);
        print_result(tests[i].cmd_str, cmd == tests[i].expected_cmd);
    }
}

// 测试错误信息转换
void test_strerror() {
    print_test_header("错误信息转换测试 (kvs_strerror)");
    
    struct {
        int errnum;
        const char* expected;
    } tests[] = {
        {KVS_OK, "OK"},
        {KVS_ERR_PARAM, "ERROR: Invalid parameter"},
        {KVS_ERR_NOMEM, "ERROR: Out of memory"},
        {KVS_ERR_NOTFOUND, "ERROR: Key not found"},
        {KVS_ERR_EXISTS, "ERROR: Key already exists"},
        {KVS_ERR_INTERNAL, "ERROR: Internal error"},
    };
    
    int num_tests = sizeof(tests)/sizeof(tests[0]);
    for(int i = 0; i < num_tests; i++) {
        const char* msg = kvs_strerror(tests[i].errnum);
        printf("错误码 %d: %s\n", tests[i].errnum, msg);
        print_result("错误信息", strcmp(msg, tests[i].expected) == 0);
    }
}

// 测试完整的协议解析器流程
void test_full_protocol() {
    print_test_header("完整协议解析器测试 (分词->识别->执行)");
    
    // 初始化全局数组
    global_array = (kvs_array_t*)malloc(sizeof(kvs_array_t));
    global_array->table = NULL;
    kvs_array_create(global_array);
    printf("✓ 初始化全局数组成功\n\n");
    
    char response[1024];
    
    // 测试1: SET命令
    printf("【测试1】SET name Alice\n");
    char msg1[] = "SET name Alice";
    char* tokens1[10] = {NULL};
    kvs_tokenizer(msg1, tokens1);
    printf("  分词结果: [%s] [%s] [%s]\n", tokens1[0], tokens1[1], tokens1[2]);
    
    int cmd1 = kvs_parser_command(tokens1);
    printf("  识别命令: %d (KVS_CMD_SET=%d)\n", cmd1, KVS_CMD_SET);
    
    memset(response, 0, sizeof(response));
    int ret1 = kvs_executor_command(cmd1, tokens1, response);
    printf("  执行结果: %s\n", response);
    print_result("SET name Alice", ret1 == KVS_OK && strcmp(response, "OK") == 0);
    
    // 测试2: GET命令
    printf("\n【测试2】GET name\n");
    char msg2[] = "GET name";
    char* tokens2[10] = {NULL};
    kvs_tokenizer(msg2, tokens2);
    printf("  分词结果: [%s] [%s]\n", tokens2[0], tokens2[1]);
    
    int cmd2 = kvs_parser_command(tokens2);
    printf("  识别命令: %d (KVS_CMD_GET=%d)\n", cmd2, KVS_CMD_GET);
    
    memset(response, 0, sizeof(response));
    int ret2 = kvs_executor_command(cmd2, tokens2, response);
    printf("  执行结果: %s\n", response);
    print_result("GET name", ret2 == KVS_OK && strstr(response, "Alice") != NULL);
    
    // 测试3: 重复SET (应该返回EXISTS错误)
    printf("\n【测试3】SET name Bob (重复的key)\n");
    char msg3[] = "SET name Bob";
    char* tokens3[10] = {NULL};
    kvs_tokenizer(msg3, tokens3);
    
    int cmd3 = kvs_parser_command(tokens3);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd3, tokens3, response);
    printf("  执行结果: %s\n", response);
    print_result("重复SET检测", strstr(response, "exists") != NULL);
    
    // 测试4: MOD命令
    printf("\n【测试4】MOD name Bob\n");
    char msg4[] = "MOD name Bob";
    char* tokens4[10] = {NULL};
    kvs_tokenizer(msg4, tokens4);
    
    int cmd4 = kvs_parser_command(tokens4);
    memset(response, 0, sizeof(response));
    int ret4 = kvs_executor_command(cmd4, tokens4, response);
    printf("  执行结果: %s\n", response);
    print_result("MOD name Bob", ret4 == KVS_OK);
    
    // 测试5: 验证MOD是否成功
    printf("\n【测试5】GET name (验证MOD)\n");
    char msg5[] = "GET name";
    char* tokens5[10] = {NULL};
    kvs_tokenizer(msg5, tokens5);
    
    int cmd5 = kvs_parser_command(tokens5);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd5, tokens5, response);
    printf("  执行结果: %s\n", response);
    print_result("验证MOD结果", strstr(response, "Bob") != NULL);
    
    // 测试6: EXIST命令
    printf("\n【测试6】EXIST name\n");
    char msg6[] = "EXIST name";
    char* tokens6[10] = {NULL};
    kvs_tokenizer(msg6, tokens6);
    
    int cmd6 = kvs_parser_command(tokens6);
    memset(response, 0, sizeof(response));
    int ret6 = kvs_executor_command(cmd6, tokens6, response);
    printf("  执行结果: %s\n", response);
    print_result("EXIST name", ret6 == KVS_OK);
    
    // 测试7: DEL命令
    printf("\n【测试7】DEL name\n");
    char msg7[] = "DEL name";
    char* tokens7[10] = {NULL};
    kvs_tokenizer(msg7, tokens7);
    
    int cmd7 = kvs_parser_command(tokens7);
    memset(response, 0, sizeof(response));
    int ret7 = kvs_executor_command(cmd7, tokens7, response);
    printf("  执行结果: %s\n", response);
    print_result("DEL name", ret7 == KVS_OK);
    
    // 测试8: GET不存在的key
    printf("\n【测试8】GET name (已删除)\n");
    char msg8[] = "GET name";
    char* tokens8[10] = {NULL};
    kvs_tokenizer(msg8, tokens8);
    
    int cmd8 = kvs_parser_command(tokens8);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd8, tokens8, response);
    printf("  执行结果: %s\n", response);
    print_result("GET不存在的key", strstr(response, "not found") != NULL);
    
    // 清理
    kvs_array_destroy(global_array);
    free(global_array);
}

// 测试边界条件和异常场景
void test_edge_cases() {
    print_test_header("边界条件和异常测试");
    
    // 初始化全局数组
    global_array = (kvs_array_t*)malloc(sizeof(kvs_array_t));
    global_array->table = NULL;
    kvs_array_create(global_array);
    
    char response[1024];
    
    // 测试1: MOD不存在的key
    printf("【测试1】MOD不存在的key\n");
    char msg1[] = "MOD nonexist value";
    char* tokens1[10] = {NULL};
    kvs_tokenizer(msg1, tokens1);
    int cmd1 = kvs_parser_command(tokens1);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd1, tokens1, response);
    printf("  执行结果: %s\n", response);
    print_result("MOD不存在的key", strstr(response, "not found") != NULL);
    
    // 测试2: DEL不存在的key
    printf("\n【测试2】DEL不存在的key\n");
    char msg2[] = "DEL nonexist";
    char* tokens2[10] = {NULL};
    kvs_tokenizer(msg2, tokens2);
    int cmd2 = kvs_parser_command(tokens2);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd2, tokens2, response);
    printf("  执行结果: %s\n", response);
    print_result("DEL不存在的key", strstr(response, "not found") != NULL);
    
    // 测试3: EXIST不存在的key
    printf("\n【测试3】EXIST不存在的key\n");
    char msg3[] = "EXIST nonexist";
    char* tokens3[10] = {NULL};
    kvs_tokenizer(msg3, tokens3);
    int cmd3 = kvs_parser_command(tokens3);
    memset(response, 0, sizeof(response));
    kvs_executor_command(cmd3, tokens3, response);
    printf("  执行结果: %s\n", response);
    print_result("EXIST不存在的key", strstr(response, "not found") != NULL);
    
    // 测试4: 多个key-value对
    printf("\n【测试4】插入多个key-value对\n");
    const char* test_data[][2] = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
    };
    
    int success_count = 0;
    for(int i = 0; i < 3; i++) {
        char msg[100];
        sprintf(msg, "SET %s %s", test_data[i][0], test_data[i][1]);
        char* tokens[10] = {NULL};
        kvs_tokenizer(msg, tokens);
        int cmd = kvs_parser_command(tokens);
        memset(response, 0, sizeof(response));
        int ret = kvs_executor_command(cmd, tokens, response);
        if(ret == KVS_OK && strcmp(response, "OK") == 0) {
            success_count++;
        }
    }
    printf("  成功插入: %d/3\n", success_count);
    print_result("多个key-value插入", success_count == 3);
    
    // 测试5: 验证所有插入的数据
    printf("\n【测试5】验证所有数据\n");
    int verify_count = 0;
    for(int i = 0; i < 3; i++) {
        char msg[100];
        sprintf(msg, "GET %s", test_data[i][0]);
        char* tokens[10] = {NULL};
        kvs_tokenizer(msg, tokens);
        int cmd = kvs_parser_command(tokens);
        memset(response, 0, sizeof(response));
        int ret = kvs_executor_command(cmd, tokens, response);
        if(ret == KVS_OK && strstr(response, test_data[i][1]) != NULL) {
            verify_count++;
        }
    }
    printf("  验证成功: %d/3\n", verify_count);
    print_result("数据完整性验证", verify_count == 3);
    
    // 测试6: 连续插入测试
    printf("\n【测试6】连续插入更多数据\n");
    char msg6[] = "SET key4 value4";
    char* tokens6[10] = {NULL};
    kvs_tokenizer(msg6, tokens6);
    int cmd6 = kvs_parser_command(tokens6);
    memset(response, 0, sizeof(response));
    int ret6 = kvs_executor_command(cmd6, tokens6, response);
    
    char msg7[] = "SET key5 value5";
    char* tokens7[10] = {NULL};
    kvs_tokenizer(msg7, tokens7);
    int cmd7 = kvs_parser_command(tokens7);
    memset(response, 0, sizeof(response));
    int ret7 = kvs_executor_command(cmd7, tokens7, response);
    
    printf("  插入key4和key5\n");
    print_result("连续插入测试", ret6 == KVS_OK && ret7 == KVS_OK);
    
    // 测试7: 无效命令
    printf("\n【测试7】无效命令识别\n");
    char msg9[] = "INVALID key value";
    char* tokens9[10] = {NULL};
    kvs_tokenizer(msg9, tokens9);
    int cmd9 = kvs_parser_command(tokens9);
    printf("  无效命令返回码: %d (应为负数)\n", cmd9);
    print_result("无效命令识别", cmd9 < 0);
    
    // 清理
    kvs_array_destroy(global_array);
    free(global_array);
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   KVStore 协议解析器测试程序           ║\n");
    printf("║   测试三层架构: 分词->识别->执行        ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // 运行各项测试
    test_tokenizer();
    test_parser();
    test_strerror();
    test_full_protocol();
    test_edge_cases();
    
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║          所有测试完成！                 ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}

