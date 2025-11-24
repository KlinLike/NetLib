#include "server.h"
#include "logger.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define CONN_MAX 1000000
#define PORT_MAX 100
#define LOG_CONN_EVERY 1000
#define LOG_REQ_EVERY 1000
#define MAX_EVENTS 8192

// 使用动态分配以避免静态数组过大导致链接失败
struct conn **conn_list = NULL;
int epfd = 0;
static msg_handler global_handler = NULL;

// 性能统计
static struct {
    long long total_connections;   // 累计连接数
    long long active_connections;  // 当前活跃连接数
    long long total_requests;      // 累计请求数
    long long total_bytes_recv;    // 累计接收字节数
    long long total_bytes_sent;    // 累计发送字节数
} server_stats = {0};

// 函数声明
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
void print_stats();

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
    if (fd < 0 || fd >= CONN_MAX) return -1;
    if (!conn_list[fd]) {
        conn_list[fd] = (struct conn*)malloc(sizeof(struct conn));
        if (!conn_list[fd]) return -1;
        memset(conn_list[fd], 0, sizeof(struct conn));
    }
    conn_list[fd]->fd = fd;
    conn_list[fd]->action_cb.recv_cb = recv_cb;
    conn_list[fd]->send_cb = send_cb;
    memset(conn_list[fd]->rbuff, 0, BUF_LEN);
    conn_list[fd]->rbuff_len = 0;
    memset(conn_list[fd]->wbuff, 0, BUF_LEN);
    conn_list[fd]->wbuff_len = 0;
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
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    if (event_register(client_fd, EPOLLIN) != 0) {
        log_error("event_register failed: client_fd=%d out of range", client_fd);
        close(client_fd);
        return -1;
    }
    
    // 更新统计
    server_stats.total_connections++;
    server_stats.active_connections++;

    return 0;
}

// NOTE: 通过业务逻辑函数 handler / encode 和收发函数 recv_cb / send_cb 分开实现解耦

int recv_cb(int fd){
    if (!conn_list[fd]) return -1;
    memset(conn_list[fd]->rbuff, 0, BUF_LEN);
    conn_list[fd]->rbuff_len = read(fd, conn_list[fd]->rbuff, BUF_LEN);
    if(conn_list[fd]->rbuff_len == 0) {
        log_info("Client disconnected (fd=%d)", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        server_stats.active_connections--;
        free(conn_list[fd]);
        conn_list[fd] = NULL;
        return 0;
    }
    else if(conn_list[fd]->rbuff_len < 0) {
        log_error("Read failed (fd=%d): %s", fd, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        server_stats.active_connections--;
        free(conn_list[fd]);
        conn_list[fd] = NULL;
        return -1;
    }
    
    
    server_stats.total_bytes_recv += conn_list[fd]->rbuff_len;
    server_stats.total_requests++;
    // NOTE: 不需要清除conn_list[fd]中的数据，因为fd被重新分配后会覆盖

    // 清空写缓冲区
    memset(conn_list[fd]->wbuff, 0, BUF_LEN);
    conn_list[fd]->wbuff_len = 0;

    if (global_handler != NULL) {
        int ret = global_handler(conn_list[fd]->rbuff, conn_list[fd]->rbuff_len,
                                 conn_list[fd]->wbuff);
        if (ret < 0) {
            log_error("Handler returned error (fd=%d, ret=%d)", fd, ret);
            close(fd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            server_stats.active_connections--;
            free(conn_list[fd]);
            conn_list[fd] = NULL;
            return ret;
        }
        conn_list[fd]->wbuff_len = ret;
    } else {
        log_warn("No message handler registered, skip processing (fd=%d)", fd);
    }

    if (conn_list[fd]->wbuff_len > 0) {
        set_epoll_event(fd, EPOLLOUT, 0);
    }
    return conn_list[fd]->wbuff_len;
}

int send_cb(int fd){
    // 发送数据到客户端 -----
    if (!conn_list[fd]) return -1;
    if(conn_list[fd]->wbuff_len == 0) {
        log_warn("Client closed before sending data (fd=%d)", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        server_stats.active_connections--;
        free(conn_list[fd]);
        conn_list[fd] = NULL;
        return 0;
    }

    int n = write(fd, conn_list[fd]->wbuff, conn_list[fd]->wbuff_len);
    if(n < 0) {
        log_error("Write failed (fd=%d): %s", fd, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        server_stats.active_connections--;
        free(conn_list[fd]);
        conn_list[fd] = NULL;
        return -1;
    }
    
    
    server_stats.total_bytes_sent += n;
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
    
    // NOTICE: 不要使用SOMAXCONN，因为它是一个编译时宏，依赖系统宏通常是错误的开始！！！
    listen(sockfd, 65535);
    return sockfd;
}

// 打印服务器统计信息
void print_stats() {
    log_info("=== Server Statistics ===");
    log_info("Total Connections: %lld", server_stats.total_connections);
    log_info("Active Connections: %lld", server_stats.active_connections);
    log_info("Total Requests: %lld", server_stats.total_requests);
    log_info("Total Bytes Recv: %lld (%.2f MB)", 
               server_stats.total_bytes_recv, 
               server_stats.total_bytes_recv / 1048576.0);
    log_info("Total Bytes Sent: %lld (%.2f MB)", 
               server_stats.total_bytes_sent,
               server_stats.total_bytes_sent / 1048576.0);
    if (server_stats.total_requests > 0) {
        log_info("Avg Request Size: %.2f bytes", 
                   (double)server_stats.total_bytes_recv / server_stats.total_requests);
        log_info("Avg Response Size: %.2f bytes",
                   (double)server_stats.total_bytes_sent / server_stats.total_requests);
    }
    log_info("========================");
}

int reactor_mainloop(unsigned short port_start, int port_count, msg_handler handler){
    if(port_start < 0 || port_count <= 0){
        log_error("Invalid port range(%d, %d)", port_start, port_start + port_count);
        return -1;
    }
    
    // 动态分配连接数组
    if (conn_list == NULL) {
        conn_list = (struct conn**)calloc(CONN_MAX, sizeof(struct conn*));
        if (conn_list == NULL) {
            log_error("Failed to allocate memory for connection list");
            return -1;
        }
    }

    global_handler = handler;

    epfd = epoll_create(1);
    if(epfd < 0) {
        log_error("epoll_create failed: %s", strerror(errno));
        return -1;
    }

    

    int i = 0;
    for(i = 0; i < port_count; i++){
        int sockfd = init_server(port_start + i);
        if(sockfd < 0){
            log_error("Init server failed on port %d", port_start + i);
            return -1;
        }
        if (sockfd < 0 || sockfd >= CONN_MAX) {
            log_error("sockfd out of range: %d (CONN_MAX=%d)", sockfd, CONN_MAX);
            close(sockfd);
            return -1;
        }
        if (!conn_list[sockfd]) {
            conn_list[sockfd] = (struct conn*)malloc(sizeof(struct conn));
            if (!conn_list[sockfd]) {
                close(sockfd);
                return -1;
            }
            memset(conn_list[sockfd], 0, sizeof(struct conn));
        }
        conn_list[sockfd]->fd = sockfd;
        conn_list[sockfd]->action_cb.accept_cb = accept_cb;
        set_epoll_event(sockfd, EPOLLIN, 1);
    }
    printf("Server listening on 0.0.0.0:%d-%d (%d ports)\n", port_start, port_start + port_count - 1, port_count);
    fflush(stdout);

    // mainloop
    struct epoll_event *events_buf = (struct epoll_event*)malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    if (!events_buf) {
        return -1;
    }
    while(1){
        int nready = epoll_wait(epfd, events_buf, MAX_EVENTS, -1);
        
        

        int i = 0;
        for(i = 0; i < nready; i++){
            int fd = events_buf[i].data.fd;
            if (fd < 0 || fd >= CONN_MAX) {
                continue;
            }
            
            
            if(events_buf[i].events & EPOLLIN){
                // action_cb是union，用哪个成员名访问都一样
                // TODO: 优化提升可读性，避免accept和read混淆
                if(conn_list[fd] && conn_list[fd]->action_cb.accept_cb != NULL){
                    conn_list[fd]->action_cb.accept_cb(fd);
                    
                } else if (conn_list[fd] && conn_list[fd]->action_cb.recv_cb != NULL) {
                    conn_list[fd]->action_cb.recv_cb(fd);
                }
            }
            else if(events_buf[i].events & EPOLLOUT){
                // send_cb是conn结构体的直接成员，不在union中
                if(conn_list[fd] && conn_list[fd]->send_cb != NULL){
                    conn_list[fd]->send_cb(fd);
                }
            }
        }
    }
}
