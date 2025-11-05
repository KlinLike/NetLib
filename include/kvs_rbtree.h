#ifndef KVS_RBTREE_H
#define KVS_RBTREE_H

#include <stdlib.h>
#include <string.h>

// --- 常量定义 ---
#define RB_RED      1   // 红色节点
#define RB_BLACK    2   // 黑色节点

// --- 数据结构定义 ---

/**
 * @brief 红黑树节点结构
 */
typedef struct rbtree_node_s {
    char *key;                      // 键（动态分配）
    char *value;                    // 值（动态分配）
    struct rbtree_node_s *left;     // 左子节点
    struct rbtree_node_s *right;    // 右子节点
    struct rbtree_node_s *parent;   // 父节点
    int color;                      // 节点颜色：RB_RED 或 RB_BLACK
} rbtree_node;

/**
 * @brief 红黑树结构
 */
typedef struct rbtree_s {
    rbtree_node *root;    // 根节点
    rbtree_node *nil;     // 哨兵节点（代表所有叶子节点）
} rbtree;

// 为了与其他模块命名统一
typedef struct rbtree_s kvs_rbtree_t;

// --- 函数声明已移至 kvstore.h ---

#endif // KVS_RBTREE_H

