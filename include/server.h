// 服务器所需定义的集合
// TODO: 处理声明分散问题，最好是每个.c都有对应的.h文件，而不是把声明都堆到这儿然后就找不到
#ifndef SERVER_H
#define SERVER_H

// 缓冲区大小配置
// C1000K测试：设为1（最小内存占用，不收发数据）
// QPS测试：设为1024或更大（需要实际收发数据）
#define BUF_LEN 1024

// 协议类型枚举（内容级分发）
typedef enum {
    PROTO_UNKNOWN = 0,
    PROTO_HTTP    = 1,
    PROTO_KVS     = 2,
    PROTO_WS      = 3
} protocol_t;

typedef int (*EVENT_CALLBACK)(int fd);
// 工作流程：
// 1. recv_cv接收客户端数据到rbuff
// 2. 调用msg_handler处理业务逻辑，填充wbuff
// 3. 调用send_cb发送数据到客户端
// msg_handler是实现业务逻辑的函数，也是操作哈希、红黑树等数据结构的函数

struct conn {
    int fd;

    char rbuff[BUF_LEN];
    int rbuff_len;

    char wbuff[BUF_LEN];
    int wbuff_len;  // 需要发送的buffer长度
    int wbuff_sent; // 已经发送的buffer长度

    EVENT_CALLBACK send_cb;
    union {
        // 只有监听套接字才需要注册accept_cb，其他套接字都注册recv_cb
        EVENT_CALLBACK recv_cb;
        EVENT_CALLBACK accept_cb;
    } action_cb;

    int status;
    protocol_t protocol;
    int should_close;

    // WebSocket 相关字段
    // WebSocket的数据帧包括 帧头 + 可选的掩码 + 有效载荷（也就是实际数据）
    // rbuff存储的是完整数据帧，payload指向实际数据，这样不用每次都计算偏移
    // 所有客户端到服务器的数据都需要进行掩码处理，所有从服务器到客户端的数据不能进行掩码处理
    char* payload;  // 有效载荷指针
    char mask[4];   // 掩码
};

typedef int (*msg_handler)(struct conn *c);

// 函数声明
int reactor_mainloop(unsigned short port_start, int port_count, msg_handler handler);

// _handle: 负责解析和处理业务逻辑，将处理结果放到wbuffer中，返回数据长度
// _encode: 负责将wbuffer中的数据编码为响应数据（协议头、分包、压缩等）

// 协议处理函数
int http_handle(struct conn *c);
int ws_handle(struct conn *c);
int kvs_handle(struct conn *c);

// 协议分发器
int dispatcher_handler(struct conn *c);

#endif // SERVER_H
