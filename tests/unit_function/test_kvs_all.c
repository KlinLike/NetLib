#include "../include/kvstore.h"
#include "../include/kvs_rbtree.h"
#include "../include/kvs_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ========== 测试配置 ==========
#define TEST_BASIC_COUNT    10      // 基础功能测试的键值对数量
#define TEST_STRESS_INSERT  1000    // 压力测试：插入数量
#define TEST_STRESS_MODIFY  500     // 压力测试：修改数量
#define TEST_STRESS_DELETE  500     // 压力测试：删除数量

// 可以通过命令行参数覆盖
int g_insert_count = TEST_STRESS_INSERT;
int g_modify_count = TEST_STRESS_MODIFY;
int g_delete_count = TEST_STRESS_DELETE;

// ========== 性能统计结构 ==========
typedef struct {
    char name[32];
    double insert_time;
    double query_time;
    double modify_time;
    double delete_time;
    int insert_success;
    int query_success;
    int modify_success;
    int delete_success;
} perf_stats_t;

// ========== 颜色定义 ==========
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_RESET   "\033[0m"

// ========== 工具函数 ==========
void print_separator(const char* title) {
    printf("\n");
    printf(COLOR_CYAN "========================================\n");
    if (title) {
        printf("  %s\n", title);
        printf("========================================\n" COLOR_RESET);
    }
}

void print_test_header(const char* ds_name) {
    printf("\n" COLOR_BLUE "┌──────────────────────────────────────┐\n");
    printf("│  测试 %s%-30s │\n", ds_name, "");
    printf("└──────────────────────────────────────┘" COLOR_RESET "\n");
}

// ========== Array 测试函数 ==========
int test_array_basic() {
    printf("\n" COLOR_YELLOW "[基础功能测试]" COLOR_RESET "\n");
    
    kvs_array_t array;
    array.table = NULL;
    
    if (kvs_array_create(&array) != KVS_OK) {
        printf(COLOR_RED "✗ 创建失败\n" COLOR_RESET);
        return -1;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 创建成功\n");
    
    // 测试 set/get/mod/del/exist
    if (kvs_array_set(&array, "name", "张三") == KVS_OK &&
        kvs_array_set(&array, "age", "25") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Set 操作正常\n");
    }
    
    char* value = NULL;
    if (kvs_array_get(&array, "name", &value) == KVS_OK && value != NULL) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Get 操作正常 (name=%s)\n", value);
    }
    
    if (kvs_array_mod(&array, "name", "李四") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Mod 操作正常\n");
    }
    
    if (kvs_array_exist(&array, "name") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Exist 操作正常\n");
    }
    
    if (kvs_array_del(&array, "age") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Del 操作正常\n");
    }
    
    kvs_array_destroy(&array);
    printf(COLOR_GREEN "✓" COLOR_RESET " 销毁成功\n");
    
    return 0;
}

int test_array_stress(perf_stats_t* stats) {
    printf("\n" COLOR_YELLOW "[压力测试]" COLOR_RESET "\n");
    
    kvs_array_t array;
    array.table = NULL;
    
    if (kvs_array_create(&array) != KVS_OK) return -1;
    
    // 插入测试
    clock_t start = clock();
    stats->insert_success = 0;
    for (int i = 0; i < g_insert_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "value_%d", i);
        if (kvs_array_set(&array, key, val) == KVS_OK) {
            stats->insert_success++;
        }
    }
    stats->insert_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 查询测试
    start = clock();
    stats->query_success = 0;
    for (int i = 0; i < stats->insert_success; i++) {
        char key[32];
        char* val = NULL;
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_array_get(&array, key, &val) == KVS_OK) {
            stats->query_success++;
        }
    }
    stats->query_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 修改测试
    start = clock();
    stats->modify_success = 0;
    int mod_count = (g_modify_count < stats->insert_success) ? g_modify_count : stats->insert_success;
    for (int i = 0; i < mod_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "modified_%d", i);
        if (kvs_array_mod(&array, key, val) == KVS_OK) {
            stats->modify_success++;
        }
    }
    stats->modify_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 删除测试
    start = clock();
    stats->delete_success = 0;
    int del_count = (g_delete_count < stats->insert_success) ? g_delete_count : stats->insert_success;
    for (int i = 0; i < del_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_array_del(&array, key) == KVS_OK) {
            stats->delete_success++;
        }
    }
    stats->delete_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    kvs_array_destroy(&array);
    return 0;
}

// ========== RBTree 测试函数 ==========
int test_rbtree_basic() {
    printf("\n" COLOR_YELLOW "[基础功能测试]" COLOR_RESET "\n");
    
    kvs_rbtree_t tree;
    tree.root = NULL;
    tree.nil = NULL;
    
    if (kvs_rbtree_create(&tree) != KVS_OK) {
        printf(COLOR_RED "✗ 创建失败\n" COLOR_RESET);
        return -1;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 创建成功\n");
    
    if (kvs_rbtree_set(&tree, "name", "张三") == KVS_OK &&
        kvs_rbtree_set(&tree, "age", "25") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Set 操作正常\n");
    }
    
    char* value = NULL;
    if (kvs_rbtree_get(&tree, "name", &value) == KVS_OK && value != NULL) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Get 操作正常 (name=%s)\n", value);
    }
    
    if (kvs_rbtree_mod(&tree, "name", "李四") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Mod 操作正常\n");
    }
    
    if (kvs_rbtree_exist(&tree, "name") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Exist 操作正常\n");
    }
    
    if (kvs_rbtree_del(&tree, "age") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Del 操作正常\n");
    }
    
    kvs_rbtree_destroy(&tree);
    printf(COLOR_GREEN "✓" COLOR_RESET " 销毁成功\n");
    
    return 0;
}

int test_rbtree_stress(perf_stats_t* stats) {
    printf("\n" COLOR_YELLOW "[压力测试]" COLOR_RESET "\n");
    
    kvs_rbtree_t tree;
    tree.root = NULL;
    tree.nil = NULL;
    
    if (kvs_rbtree_create(&tree) != KVS_OK) return -1;
    
    // 插入测试
    clock_t start = clock();
    stats->insert_success = 0;
    for (int i = 0; i < g_insert_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "value_%d", i);
        if (kvs_rbtree_set(&tree, key, val) == KVS_OK) {
            stats->insert_success++;
        }
    }
    stats->insert_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 查询测试
    start = clock();
    stats->query_success = 0;
    for (int i = 0; i < stats->insert_success; i++) {
        char key[32];
        char* val = NULL;
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_rbtree_get(&tree, key, &val) == KVS_OK) {
            stats->query_success++;
        }
    }
    stats->query_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 修改测试
    start = clock();
    stats->modify_success = 0;
    int mod_count = (g_modify_count < stats->insert_success) ? g_modify_count : stats->insert_success;
    for (int i = 0; i < mod_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "modified_%d", i);
        if (kvs_rbtree_mod(&tree, key, val) == KVS_OK) {
            stats->modify_success++;
        }
    }
    stats->modify_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 删除测试
    start = clock();
    stats->delete_success = 0;
    int del_count = (g_delete_count < stats->insert_success) ? g_delete_count : stats->insert_success;
    for (int i = 0; i < del_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_rbtree_del(&tree, key) == KVS_OK) {
            stats->delete_success++;
        }
    }
    stats->delete_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    kvs_rbtree_destroy(&tree);
    return 0;
}

// ========== Hash 测试函数 ==========
int test_hash_basic() {
    printf("\n" COLOR_YELLOW "[基础功能测试]" COLOR_RESET "\n");
    
    hashtable_t hash;
    hash.nodes = NULL;
    hash.max_slots = 0;
    hash.count = 0;
    
    if (kvs_hash_create(&hash) != KVS_OK) {
        printf(COLOR_RED "✗ 创建失败\n" COLOR_RESET);
        return -1;
    }
    printf(COLOR_GREEN "✓" COLOR_RESET " 创建成功\n");
    
    if (kvs_hash_set(&hash, "name", "张三") == KVS_OK &&
        kvs_hash_set(&hash, "age", "25") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Set 操作正常\n");
    }
    
    char* value = NULL;
    if (kvs_hash_get(&hash, "name", &value) == KVS_OK && value != NULL) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Get 操作正常 (name=%s)\n", value);
    }
    
    if (kvs_hash_mod(&hash, "name", "李四") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Mod 操作正常\n");
    }
    
    if (kvs_hash_exist(&hash, "name") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Exist 操作正常\n");
    }
    
    if (kvs_hash_del(&hash, "age") == KVS_OK) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Del 操作正常\n");
    }
    
    kvs_hash_destroy(&hash);
    printf(COLOR_GREEN "✓" COLOR_RESET " 销毁成功\n");
    
    return 0;
}

int test_hash_stress(perf_stats_t* stats) {
    printf("\n" COLOR_YELLOW "[压力测试]" COLOR_RESET "\n");
    
    hashtable_t hash;
    hash.nodes = NULL;
    hash.max_slots = 0;
    hash.count = 0;
    
    if (kvs_hash_create(&hash) != KVS_OK) return -1;
    
    // 插入测试
    clock_t start = clock();
    stats->insert_success = 0;
    for (int i = 0; i < g_insert_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "value_%d", i);
        if (kvs_hash_set(&hash, key, val) == KVS_OK) {
            stats->insert_success++;
        }
    }
    stats->insert_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 查询测试
    start = clock();
    stats->query_success = 0;
    for (int i = 0; i < stats->insert_success; i++) {
        char key[32];
        char* val = NULL;
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_hash_get(&hash, key, &val) == KVS_OK) {
            stats->query_success++;
        }
    }
    stats->query_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 修改测试
    start = clock();
    stats->modify_success = 0;
    int mod_count = (g_modify_count < stats->insert_success) ? g_modify_count : stats->insert_success;
    for (int i = 0; i < mod_count; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "modified_%d", i);
        if (kvs_hash_mod(&hash, key, val) == KVS_OK) {
            stats->modify_success++;
        }
    }
    stats->modify_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    // 删除测试
    start = clock();
    stats->delete_success = 0;
    int del_count = (g_delete_count < stats->insert_success) ? g_delete_count : stats->insert_success;
    for (int i = 0; i < del_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        if (kvs_hash_del(&hash, key) == KVS_OK) {
            stats->delete_success++;
        }
    }
    stats->delete_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    
    kvs_hash_destroy(&hash);
    return 0;
}

// ========== 性能对比输出 ==========
void print_performance_comparison(perf_stats_t* stats, int count) {
    print_separator("性能对比报告");
    
    printf("\n" COLOR_CYAN "测试配置:" COLOR_RESET "\n");
    printf("  插入数量: %d 条键值对\n", g_insert_count);
    printf("  修改数量: %d 条（从已插入的数据中选择）\n", g_modify_count);
    printf("  删除数量: %d 条（从已插入的数据中选择）\n", g_delete_count);
    printf("\n" COLOR_YELLOW "  💡 提示: 可通过 -i/-m/-d 参数调整测试规模" COLOR_RESET "\n");
    printf(COLOR_YELLOW "  例如: ./test_kvs_all.sh -i 10000 -m 5000 -d 5000" COLOR_RESET "\n");
    
    printf("\n" COLOR_CYAN "┌────────────┬──────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 数据结构   │  插入    │  查询    │  修改    │  删除    │\n");
    printf("├────────────┼──────────┼──────────┼──────────┼──────────┤" COLOR_RESET "\n");
    
    for (int i = 0; i < count; i++) {
        printf("│ %-10s │ %7.2fms │ %7.2fms │ %7.2fms │ %7.2fms │\n",
               stats[i].name,
               stats[i].insert_time,
               stats[i].query_time,
               stats[i].modify_time,
               stats[i].delete_time);
    }
    
    printf(COLOR_CYAN "└────────────┴──────────┴──────────┴──────────┴──────────┘" COLOR_RESET "\n");
    
    // 每操作平均时间
    printf("\n" COLOR_CYAN "┌────────────┬──────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 平均/操作  │  插入    │  查询    │  修改    │  删除    │\n");
    printf("├────────────┼──────────┼──────────┼──────────┼──────────┤" COLOR_RESET "\n");
    
    for (int i = 0; i < count; i++) {
        printf("│ %-10s │ %7.4fms │ %7.4fms │ %7.4fms │ %7.4fms │\n",
               stats[i].name,
               stats[i].insert_time / stats[i].insert_success,
               stats[i].query_time / stats[i].query_success,
               stats[i].modify_time / stats[i].modify_success,
               stats[i].delete_time / stats[i].delete_success);
    }
    
    printf(COLOR_CYAN "└────────────┴──────────┴──────────┴──────────┴──────────┘" COLOR_RESET "\n");
    
    // 找出最快的
    printf("\n" COLOR_YELLOW "🏆 性能冠军:" COLOR_RESET "\n");
    
    int fastest_insert = 0, fastest_query = 0, fastest_modify = 0, fastest_delete = 0;
    for (int i = 1; i < count; i++) {
        if (stats[i].insert_time < stats[fastest_insert].insert_time) fastest_insert = i;
        if (stats[i].query_time < stats[fastest_query].query_time) fastest_query = i;
        if (stats[i].modify_time < stats[fastest_modify].modify_time) fastest_modify = i;
        if (stats[i].delete_time < stats[fastest_delete].delete_time) fastest_delete = i;
    }
    
    printf("  插入最快: " COLOR_GREEN "%s" COLOR_RESET " (%.2fms)\n", 
           stats[fastest_insert].name, stats[fastest_insert].insert_time);
    printf("  查询最快: " COLOR_GREEN "%s" COLOR_RESET " (%.2fms)\n", 
           stats[fastest_query].name, stats[fastest_query].query_time);
    printf("  修改最快: " COLOR_GREEN "%s" COLOR_RESET " (%.2fms)\n", 
           stats[fastest_modify].name, stats[fastest_modify].modify_time);
    printf("  删除最快: " COLOR_GREEN "%s" COLOR_RESET " (%.2fms)\n", 
           stats[fastest_delete].name, stats[fastest_delete].delete_time);
}

// ========== 帮助信息 ==========
void print_usage(const char* prog) {
    printf("用法: %s [选项]\n\n", prog);
    printf("选项:\n");
    printf("  -i N    设置插入测试数量 (默认: %d)\n", TEST_STRESS_INSERT);
    printf("  -m N    设置修改测试数量 (默认: %d)\n", TEST_STRESS_MODIFY);
    printf("  -d N    设置删除测试数量 (默认: %d)\n", TEST_STRESS_DELETE);
    printf("  -h      显示此帮助信息\n\n");
    printf("示例:\n");
    printf("  %s -i 10000 -m 5000 -d 5000\n", prog);
    printf("  %s -i 100\n", prog);
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            g_insert_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            g_modify_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            g_delete_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    print_separator("KVS 数据结构统一测试");
    printf("\n");
    printf(COLOR_CYAN "  本测试将对比三种数据结构的性能:\n");
    printf("  • Array   - 数组实现\n");
    printf("  • RBTree  - 红黑树实现\n");
    printf("  • Hash    - 哈希表实现\n" COLOR_RESET);
    
    perf_stats_t stats[3];
    int stats_idx = 0;
    
    // 测试 Array
    print_test_header("Array");
    if (test_array_basic() == 0) {
        strcpy(stats[stats_idx].name, "Array");
        if (test_array_stress(&stats[stats_idx]) == 0) {
            printf(COLOR_GREEN "\n✓ Array 测试完成\n" COLOR_RESET);
            stats_idx++;
        }
    }
    
    // 测试 RBTree
    print_test_header("RBTree");
    if (test_rbtree_basic() == 0) {
        strcpy(stats[stats_idx].name, "RBTree");
        if (test_rbtree_stress(&stats[stats_idx]) == 0) {
            printf(COLOR_GREEN "\n✓ RBTree 测试完成\n" COLOR_RESET);
            stats_idx++;
        }
    }
    
    // 测试 Hash
    print_test_header("Hash");
    if (test_hash_basic() == 0) {
        strcpy(stats[stats_idx].name, "Hash");
        if (test_hash_stress(&stats[stats_idx]) == 0) {
            printf(COLOR_GREEN "\n✓ Hash 测试完成\n" COLOR_RESET);
            stats_idx++;
        }
    }
    
    // 输出性能对比
    print_performance_comparison(stats, stats_idx);
    
    print_separator(NULL);
    printf(COLOR_GREEN "🎉 所有测试完成！\n" COLOR_RESET);
    
    return 0;
}

