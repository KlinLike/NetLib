#include "../include/kvstore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 内存管理函数实现
void kvs_free(void* ptr){
    free(ptr);
}

void* kvs_malloc(size_t size){
    return malloc(size);
}

int main(){
    printf("=== KVS Array 测试程序 ===\n\n");
    
    // 创建数组
    kvs_array_t array;
    array.table = NULL;  // 初始化
    
    int ret = kvs_array_create(&array);
    if(ret == KVS_OK){
        printf("✓ 创建数组成功\n");
    } else {
        printf("✗ 创建数组失败: %d\n", ret);
        return 1;
    }
    
    // 测试 set
    ret = kvs_array_set(&array, "name", "张三");
    if(ret == KVS_OK){
        printf("✓ 设置 name=张三 成功\n");
    } else {
        printf("✗ 设置失败: %d\n", ret);
    }
    
    ret = kvs_array_set(&array, "age", "25");
    if(ret == KVS_OK){
        printf("✓ 设置 age=25 成功\n");
    } else {
        printf("✗ 设置失败: %d\n", ret);
    }
    
    // 测试 get
    char* value = NULL;
    ret = kvs_array_get(&array, "name", &value);
    if(ret == KVS_OK){
        printf("✓ 获取 name=%s\n", value);
    } else {
        printf("✗ 获取失败: %d\n", ret);
    }
    
    ret = kvs_array_get(&array, "age", &value);
    if(ret == KVS_OK){
        printf("✓ 获取 age=%s\n", value);
    } else {
        printf("✗ 获取失败: %d\n", ret);
    }
    
    // 测试 exist
    ret = kvs_array_exist(&array, "name");
    if(ret == KVS_OK){
        printf("✓ name 存在\n");
    } else {
        printf("✗ name 不存在\n");
    }
    
    ret = kvs_array_exist(&array, "notexist");
    if(ret == KVS_ERR_NOTFOUND){
        printf("✓ notexist 不存在（正确）\n");
    } else {
        printf("✗ 检查失败: %d\n", ret);
    }
    
    // 测试 mod
    ret = kvs_array_mod(&array, "name", "李四");
    if(ret == KVS_OK){
        printf("✓ 修改 name=李四 成功\n");
    } else {
        printf("✗ 修改失败: %d\n", ret);
    }
    
    ret = kvs_array_get(&array, "name", &value);
    if(ret == KVS_OK){
        printf("✓ 验证修改后 name=%s\n", value);
    }
    
    // 测试 del
    ret = kvs_array_del(&array, "age");
    if(ret == KVS_OK){
        printf("✓ 删除 age 成功\n");
    } else {
        printf("✗ 删除失败: %d\n", ret);
    }
    
    ret = kvs_array_exist(&array, "age");
    if(ret == KVS_ERR_NOTFOUND){
        printf("✓ 验证 age 已删除\n");
    } else {
        printf("✗ age 仍然存在\n");
    }
    
    // 销毁数组
    ret = kvs_array_destroy(&array);
    if(ret == KVS_OK){
        printf("✓ 销毁数组成功\n");
    } else {
        printf("✗ 销毁失败: %d\n", ret);
    }
    
    printf("\n=== 功能测试完成 ===\n");
    
    // ========================================
    // 压力测试
    // ========================================
    printf("\n=== 开始压力测试 ===\n\n");
    
    // 创建新数组用于压力测试
    kvs_array_t stress_array;
    stress_array.table = NULL;
    
    ret = kvs_array_create(&stress_array);
    if(ret != KVS_OK){
        printf("✗ 压力测试：创建数组失败\n");
        return 1;
    }
    
    // 测试1: 批量插入测试
    printf("[测试1] 批量插入 %d 个键值对...\n", KVS_ARRAY_SIZE);
    clock_t start = clock();
    int success_count = 0;
    int fail_count = 0;
    
    for(int i = 0; i < KVS_ARRAY_SIZE; i++){
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "value_%d_测试数据", i);
        
        ret = kvs_array_set(&stress_array, key, val);
        if(ret == KVS_OK){
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    printf("  ✓ 插入成功: %d 个\n", success_count);
    if(fail_count > 0){
        printf("  ✗ 插入失败: %d 个\n", fail_count);
    }
    printf("  ⏱ 耗时: %.2f ms\n", time_spent);
    printf("  📊 平均: %.4f ms/op\n", time_spent / KVS_ARRAY_SIZE);
    
    // 测试2: 批量查询测试
    printf("\n[测试2] 批量查询 %d 个键值对...\n", KVS_ARRAY_SIZE);
    start = clock();
    success_count = 0;
    fail_count = 0;
    
    for(int i = 0; i < KVS_ARRAY_SIZE; i++){
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        
        char* val = NULL;
        ret = kvs_array_get(&stress_array, key, &val);
        if(ret == KVS_OK && val != NULL){
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    end = clock();
    time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    printf("  ✓ 查询成功: %d 个\n", success_count);
    if(fail_count > 0){
        printf("  ✗ 查询失败: %d 个\n", fail_count);
    }
    printf("  ⏱ 耗时: %.2f ms\n", time_spent);
    printf("  📊 平均: %.4f ms/op\n", time_spent / KVS_ARRAY_SIZE);
    
    // 测试3: 批量修改测试
    printf("\n[测试3] 批量修改 512 个键值对...\n");
    start = clock();
    success_count = 0;
    fail_count = 0;
    int mod_count = 512;
    
    for(int i = 0; i < mod_count; i++){
        char key[32], new_val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(new_val, sizeof(new_val), "modified_value_%d", i);
        
        ret = kvs_array_mod(&stress_array, key, new_val);
        if(ret == KVS_OK){
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    end = clock();
    time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    printf("  ✓ 修改成功: %d 个\n", success_count);
    if(fail_count > 0){
        printf("  ✗ 修改失败: %d 个\n", fail_count);
    }
    printf("  ⏱ 耗时: %.2f ms\n", time_spent);
    printf("  📊 平均: %.4f ms/op\n", time_spent / mod_count);
    
    // 测试4: 批量删除测试
    printf("\n[测试4] 批量删除 512 个键值对...\n");
    start = clock();
    success_count = 0;
    fail_count = 0;
    int del_count = 512;
    
    for(int i = 0; i < del_count; i++){
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        
        ret = kvs_array_del(&stress_array, key);
        if(ret == KVS_OK){
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    end = clock();
    time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    printf("  ✓ 删除成功: %d 个\n", success_count);
    if(fail_count > 0){
        printf("  ✗ 删除失败: %d 个\n", fail_count);
    }
    printf("  ⏱ 耗时: %.2f ms\n", time_spent);
    printf("  📊 平均: %.4f ms/op\n", time_spent / del_count);
    
    // 测试5: 空洞填充测试（删除后重新插入）
    printf("\n[测试5] 空洞填充测试（重新插入 512 个）...\n");
    start = clock();
    success_count = 0;
    fail_count = 0;
    
    for(int i = 0; i < del_count; i++){
        char key[32], val[64];
        snprintf(key, sizeof(key), "new_key_%d", i);
        snprintf(val, sizeof(val), "new_value_%d", i);
        
        ret = kvs_array_set(&stress_array, key, val);
        if(ret == KVS_OK){
            success_count++;
        } else {
            fail_count++;
        }
    }
    
    end = clock();
    time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    printf("  ✓ 填充成功: %d 个\n", success_count);
    if(fail_count > 0){
        printf("  ✗ 填充失败: %d 个\n", fail_count);
    }
    printf("  ⏱ 耗时: %.2f ms\n", time_spent);
    printf("  📊 平均: %.4f ms/op\n", time_spent / del_count);
    
    // 测试6: 超出容量测试
    printf("\n[测试6] 超出容量测试（数组已满，尝试插入新键）...\n");
    // 当前数组应该已满（100个原始 - 50个删除 + 50个新增 = 100个）
    // 实际上因为删除创建了空洞，新插入会填充空洞，所以还能插入
    printf("  ℹ 当前数组元素数: %d / %d\n", stress_array.count, KVS_ARRAY_SIZE);
    ret = kvs_array_set(&stress_array, "overflow_key", "overflow_value");
    if(stress_array.count >= KVS_ARRAY_SIZE){
        if(ret < 0){
            printf("  ✓ 正确拒绝超出容量的插入（错误码: %d）\n", ret);
        } else {
            printf("  ⚠ 数组已满但仍然插入成功（可能填充了空洞）\n");
        }
    } else {
        printf("  ✓ 数组未满，插入成功（元素数: %d）\n", stress_array.count);
    }
    
    // 测试7: 边界条件测试
    printf("\n[测试7] 边界条件测试...\n");
    
    // 测试空键
    ret = kvs_array_get(&stress_array, "", &value);
    if(ret == KVS_ERR_PARAM || ret == KVS_ERR_NOTFOUND){
        printf("  ✓ 空键处理正确\n");
    } else {
        printf("  ✗ 空键处理异常: %d\n", ret);
    }
    
    // 测试不存在的键
    ret = kvs_array_get(&stress_array, "nonexistent_key_12345", &value);
    if(ret == KVS_ERR_NOTFOUND){
        printf("  ✓ 不存在的键处理正确\n");
    } else {
        printf("  ✗ 不存在的键处理异常: %d\n", ret);
    }
    
    // 清理压力测试数组
    ret = kvs_array_destroy(&stress_array);
    if(ret == KVS_OK){
        printf("\n✓ 压力测试数组销毁成功\n");
    } else {
        printf("\n✗ 压力测试数组销毁失败: %d\n", ret);
    }
    
    printf("\n=== 压力测试完成 ===\n");
    printf("\n=== 所有测试完成 ===\n");
    return 0;
}
