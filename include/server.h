// 服务器所需定义的集合
// TODO: 处理声明分散问题，最好是每个.c都有对应的.h文件，而不是把声明都堆到这儿然后就找不到
#ifndef SERVER_H
#define SERVER_H

// 优化内存：减小缓冲区以支持更多并发连接
// 256字节 × 2(读写缓冲) = 512字节/连接
// 100万连接仅需 512MB 内存！
// 注意：单个请求/响应不能超过 256 字节
#define BUF_LEN 256

#define ENABLE_HTTP         0
#define ENABLE_WEBSOCKET    0
#define ENABLE_KVSTORE      1
#define ENABLE_ECHO         0

typedef int (*EVENT_CALLBACK)(int fd);
// 工作流程：
// 1. recv_cv接收客户端数据到rbuff
// 2. 调用msg_handler处理业务逻辑，填充wbuff
// 3. 调用send_cb发送数据到客户端
// msg_handler是实现业务逻辑的函数，也是操作哈希、红黑树等数据结构的函数
typedef int (*msg_handler)(char *msg, int length, char *response);

struct conn {
    int fd;

    char rbuff[BUF_LEN];
    int rbuff_len;

    char wbuff[BUF_LEN];
    int wbuff_len;

    EVENT_CALLBACK send_cb;
    union {
        // 只有监听套接字才需要注册accept_cb，其他套接字都注册recv_cb
        EVENT_CALLBACK recv_cb;
        EVENT_CALLBACK accept_cb;
    } action_cb;

    int status;

#if ENABLE_WEBSOCKET
    // WebSocket的数据帧包括 帧头 + 可选的掩码 + 有效载荷（也就是实际数据）
    // rbuff存储的是完整数据帧，payload指向实际数据，这样不用每次都计算偏移
    // 所有客户端到服务器的数据都需要进行掩码处理，所有从服务器到客户端的数据不能进行掩码处理，掩码处理使用XOR运算
    char* payload;  // 有效载荷
    char mask[4];   // 掩码
#endif
};

// 函数声明
int reactor_mainloop(unsigned short port_start, int port_count, msg_handler handler);

// _handle 
// 功能：负责解析和处理业务逻辑，将处理结果放到wbuffer中
// 返回值：处理后的数据长度
// _encode 
// 功能：负责将wbuffer中的数据编码为响应数据，并发送给客户端，需要处理协议头、分包、压缩等额外工作。处理后的数据还是保存在wbuffer中
// 返回值：处理后的数据长度

#if ENABLE_HTTP
int http_handle(struct conn *c);
int http_encode(struct conn *c);
#endif

#if ENABLE_WEBSOCKET
int ws_handle(struct conn *c);
int ws_encode(struct conn *c);
#endif

#if ENABLE_KVSTORE
int kvs_handle(struct conn *c);
int kvs_encode(struct conn *c);
#endif

#if ENABLE_ECHO
int echo_handle(struct conn *c);
int echo_encode(struct conn *c);
#endif

#endif // SERVER_H