#include "server.h"
#include "logger.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define CONN_MAX 1024
#define PORT_MAX 20

struct conn conn_list[CONN_MAX];
int epfd = 0;

// 函数声明
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

// NOTE:
//  reactor基于EPOLL实现

// 设置EPOLL事件
int set_epoll_event(int fd, int event, int isAdd){
    struct epoll_event ev;
    ev.events = event; // 设置要监控的事件类型
    ev.data.fd = fd;
    log_debug("set_epoll_event: epfd=%d, fd=%d, event=0x%x, %s", 
              epfd, fd, event, isAdd ? "ADD" : "MOD");
    
    int ret = epoll_ctl(epfd, isAdd ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ev);
    if(ret < 0) {
        log_error("epoll_ctl failed: fd=%d, errno=%d (%s)", fd, errno, strerror(errno));
    }
    return ret;
}

// 初始化连接数据并注册EPOLL
int event_register(int fd, int event) {
	if (fd < 0) return -1;

	conn_list[fd].fd = fd;
	conn_list[fd].action_cb.recv_cb = recv_cb; // 只有监听套接字才需要注册accept_cb，其他套接字都注册recv_cb
	conn_list[fd].send_cb = send_cb;

	memset(conn_list[fd].rbuff, 0, BUF_LEN);
	conn_list[fd].rbuff_len = 0;

	memset(conn_list[fd].wbuff, 0, BUF_LEN);
	conn_list[fd].wbuff_len = 0;

	set_epoll_event(fd, event, 1);
	return 0;
}

// ----- 回调函数 -----

int accept_cb(int fd){
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    // fd是监听套接字，client_fd是客户端套接字
    int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if(client_fd < 0) {
        log_error("accept failed: %s", strerror(errno));
        return -1;
    }
    
    // 记录新连接日志
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    log_connection("New client connected: %s:%d (fd=%d)", 
                   client_ip, ntohs(client_addr.sin_port), client_fd);
    
    event_register(client_fd, EPOLLIN);

    return 0;
}

// NOTE:
// 通过业务逻辑函数 handler / encode 和收发函数 recv_cb / send_cb 分开实现解耦

int recv_cb(int fd){
    // 接收数据到rbuff中 -----
    memset(conn_list[fd].rbuff, 0, BUF_LEN);
    conn_list[fd].rbuff_len = read(fd, conn_list[fd].rbuff, BUF_LEN);
    if(conn_list[fd].rbuff_len == 0) {
        log_info("Client disconnected (fd=%d)", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    }
    else if(conn_list[fd].rbuff_len < 0) {
        log_error("Read failed (fd=%d): %s", fd, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return -1;
    }
    
    // 记录接收到的数据
    log_debug("Received %d bytes from fd=%d", conn_list[fd].rbuff_len, fd);
    // 不需要清除conn_list[fd]中的数据，因为fd被重新分配后会覆盖

    // 处理业务逻辑 -----
#if ENABLE_KVSTORE
    kvs_handle(&conn_list[fd]);
#endif

#if ENABLE_ECHO
    echo_handle(&conn_list[fd]);
#endif
#if ENABLE_HTTP
    http_handle(&conn_list[fd]);
#endif
#if ENABLE_WEBSOCKET
    ws_handle(&conn_list[fd]);
#endif

    // 设置EPOLLOUT事件，准备发送数据 -----
    set_epoll_event(fd, EPOLLOUT, 0);
    return conn_list[fd].rbuff_len;
}

int send_cb(int fd){
    // 准备发送数据 -----
#if ENABLE_KVSTORE
    kvs_encode(&conn_list[fd]);
#endif
#if ENABLE_ECHO
    echo_encode(&conn_list[fd]);
#endif
#if ENABLE_HTTP
    http_encode(&conn_list[fd]);
#endif
#if ENABLE_WEBSOCKET
    ws_encode(&conn_list[fd]);
#endif
    
    // 发送数据到客户端 -----
    if(conn_list[fd].wbuff_len == 0) {
        log_warn("Client closed before sending data (fd=%d)", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    }

    int n = write(fd, conn_list[fd].wbuff, conn_list[fd].wbuff_len);
    if(n < 0) {
        log_error("Write failed (fd=%d): %s", fd, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return -1;
    }
    
    log_debug("Sent %d bytes to fd=%d", n, fd);
    set_epoll_event(fd, EPOLLIN, 0);
    return n;
}

// 打开一个服务器套接字并监听端口，返回套接字fd
int init_server(unsigned short port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        log_error("socket creation failed: %s", strerror(errno));
        return -1;
    }
    // 设置 SO_REUSEADDR 选项，允许服务器在TIME_WAIT状态下立即重启
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("bind failed on port %d: %s", port, strerror(errno));
        return -1;
    }
    listen(sockfd, 128);
    return sockfd;
}

#if ENABLE_KVSTORE
static msg_handler kvs_handler;
#endif

int reactor_mainloop(unsigned short port_start, int port_count, msg_handler handler){
    if(port_start < 0 || port_count <= 0){
        log_error("Invalid port range(%d, %d)", port_start, port_start + port_count);
        return -1;
    }

#if ENABLE_KVSTORE
    kvs_handler = handler;
#else
    (void)handler;  // 避免未使用参数警告
#endif

    epfd = epoll_create(1);
    if(epfd < 0) {
        log_error("epoll_create failed: %s", strerror(errno));
        return -1;
    }
    log_debug("epoll_create success, epfd=%d", epfd);

    int i = 0;
    for(i = 0; i < port_count; i++){
        int sockfd = init_server(port_start + i);
        if(sockfd < 0){
            log_error("Init server failed on port %d", port_start + i);
            return -1;
        }
        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].action_cb.accept_cb = accept_cb;
        set_epoll_event(sockfd, EPOLLIN, 1);
        log_server("Listening on 0.0.0.0:%d", port_start + i);
    }
    
    log_server("Echo server started successfully!");
    log_server("Waiting for connections...");

    // mainloop
    while(1){
        struct epoll_event events[CONN_MAX];
        int nready = epoll_wait(epfd, events, CONN_MAX, -1);
        
        log_debug("epoll_wait returned %d events", nready);

        int i = 0;
        for(i = 0; i < nready; i++){
            int fd = events[i].data.fd;
            log_debug("Event on fd=%d, events=0x%x", fd, events[i].events);
            
            if(events[i].events & EPOLLIN){
                // action_cb是union，可能是accept_cb或recv_cb
                if(conn_list[fd].action_cb.accept_cb != NULL){
                    conn_list[fd].action_cb.accept_cb(fd);
                }
            }
            else if(events[i].events & EPOLLOUT){
                // send_cb是conn结构体的直接成员，不在union中
                if(conn_list[fd].send_cb != NULL){
                    conn_list[fd].send_cb(fd);
                }
            }
        }
    }
}