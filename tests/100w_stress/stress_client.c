// NetLib C 压测客户端（10万并发首版）
// 目标：分批非阻塞建连，维持长连接到 KVS 服务器，统计成功数
// 用法示例：
//   ./stress_client --host 127.0.0.1 --port-range 2000-2099 --connections 100000 --batch 10000 --interval-ms 100

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <time.h>

typedef struct Conn {
    int fd;
    int connected;
    int port;
} Conn;

static int set_nonblock(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int parse_port_range(const char *s, int *start, int *end){
    int a=0,b=0;
    if(sscanf(s, "%d-%d", &a, &b) == 2 && a>0 && b>=a){
        *start=a; *end=b; return 0;
    }
    return -1;
}

int main(int argc, char **argv){
    const char *host = "127.0.0.1";
    const char *port_range_str = "2000-2099";
    int conn_target = 100000;
    int batch = 10000;
    int interval_ms = 100;

    for(int i=1;i<argc;i++){
        if(strcmp(argv[i], "--host")==0 && i+1<argc) host=argv[++i];
        else if(strcmp(argv[i], "--port-range")==0 && i+1<argc) port_range_str=argv[++i];
        else if(strcmp(argv[i], "--connections")==0 && i+1<argc) conn_target=atoi(argv[++i]);
        else if(strcmp(argv[i], "--batch")==0 && i+1<argc) batch=atoi(argv[++i]);
        else if(strcmp(argv[i], "--interval-ms")==0 && i+1<argc) interval_ms=atoi(argv[++i]);
    }

    int port_start=0, port_end=0;
    if(parse_port_range(port_range_str, &port_start, &port_end) != 0){
        fprintf(stderr, "Invalid port range: %s\n", port_range_str);
        return 1;
    }
    int port_count = port_end - port_start + 1;

    Conn *conns = (Conn*)calloc(conn_target, sizeof(Conn));
    if(!conns){
        fprintf(stderr, "Failed to alloc conns\n");
        return 1;
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int epfd = epoll_create1(0);
    if(epfd < 0){
        perror("epoll_create1");
        return 1;
    }

    int created = 0, established = 0, failed = 0;

    while(created < conn_target){
        int to_create = batch;
        if(created + to_create > conn_target) to_create = conn_target - created;

        for(int k=0; k<to_create; k++){
            int idx = created + k;
            int port = port_start + (idx % port_count);

            char port_str[16];
            snprintf(port_str, sizeof(port_str), "%d", port);

            if(getaddrinfo(host, port_str, &hints, &res) != 0){
                failed++; continue;
            }
            int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if(fd < 0){ failed++; freeaddrinfo(res); continue; }
            if(set_nonblock(fd) < 0){ failed++; close(fd); freeaddrinfo(res); continue; }
            int rc = connect(fd, res->ai_addr, res->ai_addrlen);
            if(rc == 0){
                conns[idx].fd = fd; conns[idx].connected = 1; conns[idx].port = port; established++;
            } else if(errno == EINPROGRESS){
                conns[idx].fd = fd; conns[idx].connected = 0; conns[idx].port = port;
                struct epoll_event ev; ev.events = EPOLLOUT; ev.data.u64 = (uint64_t)idx;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0){ failed++; close(fd); conns[idx].fd=-1; }
            } else {
                failed++; close(fd);
            }
            freeaddrinfo(res); res=NULL;
        }

        created += to_create;
        fprintf(stdout, "[Batch] created=%d/%d, established=%d, failed=%d\n", created, conn_target, established, failed);

        struct epoll_event events[8192];
        int rounds = 5; // 若干轮尝试完成握手
        while(rounds--){
            int nfds = epoll_wait(epfd, events, 8192, 50);
            for(int i=0;i<nfds;i++){
                int idx = (int)events[i].data.u64;
                int fd = conns[idx].fd;
                int err=0; socklen_t len=sizeof(err);
                if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0){
                    conns[idx].connected = 1; established++;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                } else {
                    failed++; close(fd); conns[idx].fd=-1;
                }
            }
        }

        struct timespec ts; ts.tv_sec = interval_ms/1000; ts.tv_nsec = (interval_ms%1000)*1000000L;
        nanosleep(&ts, NULL);
    }

    fprintf(stdout, "[Summary] target=%d, established=%d, failed=%d\n", conn_target, established, failed);

    // 简单维持：睡眠一段时间保持连接（可按需扩展发送心跳）
    sleep(60);

    for(int i=0;i<conn_target;i++){
        if(conns[i].fd>0) close(conns[i].fd);
    }
    close(epfd);
    free(conns);
    return 0;
}