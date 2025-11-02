#include "../include/kvstore.h"
#include <stdio.h>
#include <stdlib.h>

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
    
    printf("\n=== 测试完成 ===\n");
    return 0;
}
