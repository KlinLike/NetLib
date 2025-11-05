#include "kvstore.h"
#include "kvs_rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ========== 红黑树哨兵节点说明 ==========
 * 
 * 哨兵节点（Sentinel Node / nil）是红黑树中用来替代所有 NULL 指针的特殊虚拟节点。
 * 
 * 【核心优势】
 * 1. 避免空指针解引用崩溃
 *    - 所有"空"位置都指向同一个有效的 nil 对象（而不是 NULL）
 *    - 即使误访问 nil->color 也不会崩溃，因为 nil 是真实存在的内存对象
 * 
 * 2. nil 可以携带属性
 *    - nil->color 总是 BLACK，符合红黑树性质
 *    - 在某些算法中可以直接访问 node->child->color，不需要先判断 child 是否为 NULL
 * 
 * 3. 统一的边界表示
 *    - 所有空位置都指向同一个 T->nil 对象
 *    - 可以用 node == T->nil 判断是否为空
 * 
 * 【重要】
 * - 使用哨兵并不会减少检查次数（从 node != NULL 变成 node != T->nil）
 * - 真正的价值在于"把 NULL 替换成可安全访问的 nil 对象"
 * 
 * 【实现要点】
 * - 创建树时分配一个 nil 节点，并设置为黑色
 * - 空树时 root == nil
 * - 所有叶子节点的 left/right 指向 nil
 * - 搜索失败时返回 nil（而不是 NULL）
 * - 删除等操作必须检查 node == T->nil，避免误操作哨兵节点
 */

// ========== 宏定义 ==========
#define ENABLE_KEY_CHAR 1   // 启用char*类型的Key
#define ENABLE_KEY_INT  0   // 启用int类型的Key

#define RED             RB_RED      // 红色结点
#define BLACK           RB_BLACK    // 黑色结点

// 为了兼容旧代码中的KEY_TYPE
typedef char* KEY_TYPE;

// ========== 全局变量 ==========
kvs_rbtree_t global_rbtree_instance;
kvs_rbtree_t* global_rbtree = &global_rbtree_instance;

// ========== 内部辅助函数 ==========

// 查找以x为根的子树中的最小节点
static rbtree_node *rbtree_mini(rbtree *T, rbtree_node *x) {
    while (x->left != T->nil) {
        x = x->left;
    }
    return x;
}

// 查找以x为根的子树中的最大节点
static rbtree_node *rbtree_maxi(rbtree *T, rbtree_node *x) {
    while (x->right != T->nil) {
        x = x->right;
    }
    return x;
}

// 查找x的后继节点
static rbtree_node *rbtree_successor(rbtree *T, rbtree_node *x) {
    rbtree_node *y = x->parent;

    if (x->right != T->nil) {
        return rbtree_mini(T, x->right);
    }

    while ((y != T->nil) && (x == y->right)) {
        x = y;
        y = y->parent;
    }
    return y;
}

// 对节点x进行左旋
static void rbtree_left_rotate(rbtree *T, rbtree_node *x) {
    rbtree_node *y = x->right;  // x  --> y  ,  y --> x,   right --> left,  left --> right

    x->right = y->left; //1 1
    if (y->left != T->nil) { //1 2
        y->left->parent = x;
    }

    y->parent = x->parent; //1 3
    if (x->parent == T->nil) { //1 4
        T->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    y->left = x; //1 5
    x->parent = y; //1 6
}

// 对节点y进行右旋
static void rbtree_right_rotate(rbtree *T, rbtree_node *y) {
    rbtree_node *x = y->left;

    y->left = x->right;
    if (x->right != T->nil) {
        x->right->parent = y;
    }

    x->parent = y->parent;
    if (y->parent == T->nil) {
        T->root = x;
    } else if (y == y->parent->right) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }

    x->right = y;
    y->parent = x;
}

// 插入节点z到红黑树T中 通过变色和旋转修复红黑树性质
static void rbtree_insert_fixup(rbtree *T, rbtree_node *z) {
    while (z->parent->color == RED) { //z ---> RED
        if (z->parent == z->parent->parent->left) {
            rbtree_node *y = z->parent->parent->right;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;

                z = z->parent->parent; //z --> RED
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rbtree_left_rotate(T, z);
                }

                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rbtree_right_rotate(T, z->parent->parent);
            }
        } else {
            rbtree_node *y = z->parent->parent->left;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;

                z = z->parent->parent; //z --> RED
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rbtree_right_rotate(T, z);
                }

                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rbtree_left_rotate(T, z->parent->parent);
            }
        }
    }

    T->root->color = BLACK;
}

// 将节点z插入到红黑树T中
static void rbtree_insert(rbtree *T, rbtree_node *z) {
    rbtree_node *y = T->nil;
    rbtree_node *x = T->root;

    while (x != T->nil) {
        y = x;
#if ENABLE_KEY_CHAR
        if (strcmp(z->key, x->key) < 0) {
            x = x->left;
        } else if (strcmp(z->key, x->key) > 0) {
            x = x->right;
        } else {
            return ;
        }
#elif ENABLE_KEY_INT
        if (z->key < x->key) {
            x = x->left;
        } else if (z->key > x->key) {
            x = x->right;
        } else { //Exist
            return ;
        }
#endif
    }

    z->parent = y;
    if (y == T->nil) {
        T->root = z;
#if ENABLE_KEY_CHAR
    } else if (strcmp(z->key, y->key) < 0) {
#elif ENABLE_KEY_INT
    } else if (z->key < y->key) {
#endif
        y->left = z;
    } else {
        y->right = z;
    }

    z->left = T->nil;
    z->right = T->nil;
    z->color = RED;

    rbtree_insert_fixup(T, z);
}

// 在删除一个黑色节点后，修复红黑树性质，保持树的平衡
static void rbtree_delete_fixup(rbtree *T, rbtree_node *x) {
    while ((x != T->root) && (x->color == BLACK)) {
        if (x == x->parent->left) {
            rbtree_node *w= x->parent->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;

                rbtree_left_rotate(T, x->parent);
                w = x->parent->right;
            }

            if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    rbtree_right_rotate(T, w);
                    w = x->parent->right;
                }

                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                rbtree_left_rotate(T, x->parent);

                x = T->root;
            }
        } else {
            rbtree_node *w = x->parent->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                rbtree_right_rotate(T, x->parent);
                w = x->parent->left;
            }

            if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    rbtree_left_rotate(T, w);
                    w = x->parent->left;
                }

                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                rbtree_right_rotate(T, x->parent);

                x = T->root;
            }
        }
    }

    x->color = BLACK;
}

// 从红黑树中删除节点 z，并调用修复函数来保持树的平衡
static rbtree_node *rbtree_delete(rbtree *T, rbtree_node *z) {
    rbtree_node *y = T->nil;
    rbtree_node *x = T->nil;

    if ((z->left == T->nil) || (z->right == T->nil)) {
        y = z;
    } else {
        y = rbtree_successor(T, z);
    }

    if (y->left != T->nil) {
        x = y->left;
    } else if (y->right != T->nil) {
        x = y->right;
    }

    x->parent = y->parent;
    if (y->parent == T->nil) {
        T->root = x;
    } else if (y == y->parent->left) {
        y->parent->left = x;
    } else {
        y->parent->right = x;
    }

    if (y != z) {
#if ENABLE_KEY_CHAR
        void *tmp = z->key;
        z->key = y->key;
        y->key = tmp;

        tmp = z->value;
        z->value= y->value;
        y->value = tmp;
#else
        z->key = y->key;
        z->value = y->value;
#endif
    }

    if (y->color == BLACK) {
        rbtree_delete_fixup(T, x);
    }

    return y;
}

// 在红黑树 T 中根据给定的 key 查找并返回对应的节点
static rbtree_node *rbtree_search(rbtree *T, KEY_TYPE key) {
    rbtree_node *node = T->root;
    while (node != T->nil) {
#if ENABLE_KEY_CHAR
        if (strcmp(key, node->key) < 0) {
            node = node->left;
        } else if (strcmp(key, node->key) > 0) {
            node = node->right;
        } else {
            return node;
        }
#else
        if (key < node->key) {
            node = node->left;
        } else if (key > node->key) {
            node = node->right;
        } else {
            return node;
        }	
#endif
    }
    return T->nil;
}

// 中序遍历红黑树并打印以node为根的子树的所有节点信息
static void rbtree_traversal(rbtree *T, rbtree_node *node) {
    if (node != T->nil) {
        rbtree_traversal(T, node->left);
#if ENABLE_KEY_CHAR
        printf("key:%s, value:%s\n", node->key, (char *)node->value);
#else
        printf("key:%d, color:%d\n", node->key, node->color);
#endif
        rbtree_traversal(T, node->right);
    }
}

// ========== KVStore 对外接口实现 ==========

/**
 * @brief 创建并初始化一个红黑树实例
 */
int kvs_rbtree_create(kvs_rbtree_t *inst) {
    if (inst == NULL) {
        return KVS_ERR_PARAM;
    }

    inst->nil = (rbtree_node*)kvs_malloc(sizeof(rbtree_node));
    if (inst->nil == NULL) {
        return KVS_ERR_NOMEM;
    }

    inst->nil->color = BLACK;
    inst->root = inst->nil;

    return KVS_OK;
}

/*
 * ========== 旧的低效实现（已弃用，仅供参考）==========
 * 
 * 问题：每次删除都调用 rbtree_delete，会维护红黑树性质（旋转、变色）
 * 但销毁时维护这些性质毫无意义，导致 O(n log n) 复杂度
 */
#if 0
int kvs_rbtree_destroy_old(kvs_rbtree_t *inst) {
    if (inst == NULL) {
        return KVS_ERR_PARAM;
    }

    if (inst->nil == NULL) {
        return KVS_OK;
    }

    // 每次找最小节点并删除 - O(n log n)
    while (inst->root != inst->nil) {
        rbtree_node *mini = rbtree_mini(inst, inst->root);
        rbtree_node *deleted = rbtree_delete(inst, mini);  // 会维护红黑树性质，浪费时间
        if (deleted != NULL && deleted != inst->nil) {
            if (deleted->key) kvs_free(deleted->key);
            if (deleted->value) kvs_free(deleted->value);
            kvs_free(deleted);
        }
    }

    kvs_free(inst->nil);
    inst->nil = NULL;
    inst->root = NULL;

    return KVS_OK;
}
#endif

/*
 * ========== 树销毁策略对比 ==========
 * 
 * 【策略1：总是删除根节点】
 * - 方法：每次删除根，根的后继成为新根
 * - 缺点：需要调用 rbtree_delete，维护红黑树性质
 * - 复杂度：O(n log n)
 * - 结论：❌ 浪费，不需要维护性质
 * 
 * 【策略2：总是删除最小/最大节点】
 * - 方法：每次找最小/最大，删除，重复
 * - 缺点：同样需要调用 rbtree_delete，维护性质
 * - 复杂度：O(n log n)
 * - 结论：❌ 浪费，不需要维护性质
 * 
 * 【策略3：后序遍历直接释放】✅ 最优选择
 * - 方法：递归遍历，左→右→根，直接释放节点
 * - 优点：不调用 rbtree_delete，不维护性质，直接释放
 * - 复杂度：O(n) - 每个节点访问一次
 * - 结论：✅ 高效，简洁，最优解
 * 
 * 【为什么选择后序遍历？】
 * 1. 必须先释放子节点，再释放父节点（否则访问不到子节点）
 * 2. 后序遍历正好符合这个顺序：左子树→右子树→根节点
 * 3. 前序（根→左→右）和中序（左→根→右）都会在释放根后无法访问子树
 * 
 * 【实现方式对比】
 * - 递归实现：代码简洁，但可能栈溢出（深度过大）
 * - 迭代实现：使用栈或队列，不会栈溢出，但代码复杂
 * - 本实现选择递归：红黑树高度 O(log n)，栈溢出风险低，代码更清晰
 */

// 辅助函数：递归释放子树（后序遍历）
static void destroy_subtree(rbtree *T, rbtree_node *node) {
    if (node == T->nil) {
        return;  // 遇到哨兵节点，返回
    }
    
    // 1. 递归释放左子树
    destroy_subtree(T, node->left);
    
    // 2. 递归释放右子树
    destroy_subtree(T, node->right);
    
    // 3. 释放当前节点（此时左右子树已释放）
    if (node->key) {
        kvs_free(node->key);
    }
    if (node->value) {
        kvs_free(node->value);
    }
    kvs_free(node);
}

/**
 * @brief 销毁一个红黑树实例，释放其占用的所有内存
 * 
 * 实现策略：后序遍历（左→右→根）直接释放节点
 * 时间复杂度：O(n)，每个节点访问一次
 * 空间复杂度：O(log n)，递归栈深度
 */
int kvs_rbtree_destroy(kvs_rbtree_t *inst) {
    if (inst == NULL) {
        return KVS_ERR_PARAM;
    }

    if (inst->nil == NULL) {
        return KVS_OK;  // 已经销毁或未初始化
    }

    // 后序遍历释放所有节点（不维护红黑树性质）
    destroy_subtree(inst, inst->root);

    // 释放哨兵节点
    kvs_free(inst->nil);
    inst->nil = NULL;
    inst->root = NULL;

    return KVS_OK;
}

/**
 * @brief 根据 key 获取对应的 value
 */
int kvs_rbtree_get(kvs_rbtree_t *inst, char *key, char **value) {
    if (inst == NULL || key == NULL || value == NULL) {
        return KVS_ERR_PARAM;
    }

    *value = NULL;

    rbtree_node *node = rbtree_search(inst, key);
    if (node == NULL || node == inst->nil) {
        return KVS_ERR_NOTFOUND;
    }

    *value = node->value;
    return KVS_OK;
}

/**
 * @brief 向红黑树中设置一个新的键值对
 */
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value) {
    if (inst == NULL || key == NULL || value == NULL) {
        return KVS_ERR_PARAM;
    }

    // 检查 key 是否已存在
    rbtree_node *existing = rbtree_search(inst, key);
    if (existing != NULL && existing != inst->nil) {
        return KVS_ERR_EXISTS;
    }

    // 创建新节点
    rbtree_node *node = (rbtree_node*)kvs_malloc(sizeof(rbtree_node));
    if (node == NULL) {
        return KVS_ERR_NOMEM;
    }

    // 分配并复制 key
    node->key = kvs_malloc(strlen(key) + 1);
    if (node->key == NULL) {
        kvs_free(node);
        return KVS_ERR_NOMEM;
    }
    memset(node->key, 0, strlen(key) + 1);
    strcpy(node->key, key);

    // 分配并复制 value
    node->value = kvs_malloc(strlen(value) + 1);
    if (node->value == NULL) {
        kvs_free(node->key);
        kvs_free(node);
        return KVS_ERR_NOMEM;
    }
    memset(node->value, 0, strlen(value) + 1);
    strcpy(node->value, value);

    // 插入红黑树
    rbtree_insert(inst, node);

    return KVS_OK;
}

/**
 * @brief 根据 key 删除一个键值对
 */
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key) {
    if (inst == NULL || key == NULL) {
        return KVS_ERR_PARAM;
    }

    rbtree_node *node = rbtree_search(inst, key);
    if (node == NULL || node == inst->nil) {
        return KVS_ERR_NOTFOUND;
    }

    rbtree_node *deleted = rbtree_delete(inst, node);
    if (deleted != NULL && deleted != inst->nil) {
        if (deleted->key) kvs_free(deleted->key);
        if (deleted->value) kvs_free(deleted->value);
        kvs_free(deleted);
    }

    return KVS_OK;
}

/**
 * @brief 修改一个已存在的 key 所对应的 value
 */
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value) {
    if (inst == NULL || key == NULL || value == NULL) {
        return KVS_ERR_PARAM;
    }

    rbtree_node *node = rbtree_search(inst, key);
    if (node == NULL || node == inst->nil) {
        return KVS_ERR_NOTFOUND;
    }

    // 释放旧值
    if (node->value != NULL) {
        kvs_free(node->value);
    }

    // 分配并复制新值
    node->value = kvs_malloc(strlen(value) + 1);
    if (node->value == NULL) {
        return KVS_ERR_NOMEM;
    }
    memset(node->value, 0, strlen(value) + 1);
    strcpy(node->value, value);

    return KVS_OK;
}

/**
 * @brief 检查一个 key 是否存在
 */
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key) {
    if (inst == NULL || key == NULL) {
        return KVS_ERR_PARAM;
    }

    char *value = NULL;
    int ret = kvs_rbtree_get(inst, key, &value);
    return ret;  // 存在返回 KVS_OK，不存在返回 KVS_ERR_NOTFOUND
}
