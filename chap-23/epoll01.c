#include  <sys/epoll.h>
#include "lib/common.h"

#define MAXEVENTS 128

char rot13_char(char c) {
    if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M'))
        return c + 13;
    else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z'))
        return c - 13;
    else
        return c;
}

/**
 * 核心三个函数 epoll_create  epoll_ctl  epoll_wait
 * 
 * epoll 
 *   - 默认的 level-triggered（条件触发）机制
 *   - 提供了性能更为强劲的 edge-triggered（边缘触发）机制
 * 
 *  int epoll_create(int size);
 *  int epoll_create1(int flags);
 *      返回值: 若成功返回一个大于 0 的值，表示 epoll 实例；若返回 -1 表示出 
 *  
 *  epoll_ctl 在 epoll_create 完成后, 可以往这个 epoll 实例增加或删除监控的事件
 *  int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
 *      epfd: epoll_create 创建的 epoll 实例描述字
        返回值: 若成功返回 0；若返回 -1 表示出错
            EPOLL_CTL_ADD
            EPOLL_CTL_DEL
            EPOLL_CTL_MOD
        三个参数是注册的事件的文件描述符，比如一个监听套接字
        第四个参数表示的是注册的事件类型，并且可以在这个结构体里设置用户需要的数据

   epoll_wait() 函数类似之前的 poll 和 select 函数，调用者进程被挂起，在等待内核 I/O 事件的分发
   
   int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
      返回值: 成功返回的是一个大于 0 的数，表示事件的个数；返回 0 表示的是超时时间到；若出错返回 -1.

    事件类型
 * EPOLLIN：表示对应的文件描述字可以读；
 * EPOLLOUT：表示对应的文件描述字可以写；
 * EPOLLRDHUP：表示套接字的一端已经关闭，或者半关闭；
 * EPOLLHUP：表示对应的文件描述字被挂起；
 * EPOLLET：设置为 edge-triggered，默认为 level-triggered
 */
int main(int argc, char **argv) {
    int listen_fd, socket_fd;
    int n, i;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;

    listen_fd = tcp_nonblocking_server_listen(SERV_PORT);

    // 创建 epoll 实例
    efd = epoll_create1(0);
    if (efd == -1) {
        error(1, errno, "epoll create failed");
    }

    // 调用 epoll_ctl 将监听套接字对应的 I/O 事件进行了注册，这样在有新的连接建立之后，就可以感知到。
    // 注意这里使用的是 edge-triggered（边缘触发）
    event.data.fd = listen_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        error(1, errno, "epoll_ctl add listen fd failed");
    }

    /* Buffer where events are returned */
    // 为返回的 event 数组分配了内存
    events = calloc(MAXEVENTS, sizeof(event));

    while (1) {
        // 当 epoll_wait 成功返回时，通过遍历返回的 event 数组，就直接可以知道发生的 I/O 事件
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        printf("epoll_wait wakeup\n");
        for (i = 0; i < n; i++) {
            // 对各种错误的情况进行判断
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            } else if (listen_fd == events[i].data.fd) {
                // 监听套接字有事件
                struct sockaddr_storage ss;
                socklen_t slen = sizeof(ss);
                // 调用 accept 建立连接 
                int fd = accept(listen_fd, (struct sockaddr *) &ss, &slen);
                if (fd < 0) {
                    error(1, errno, "accept failed");
                } else {
                    // 并将该连接设置为非阻塞,
                    make_nonblocking(fd);
                    event.data.fd = fd;
                    event.events = EPOLLIN | EPOLLET; //edge-triggered
                    // 再调用 epoll_ctl 把已连接套接字对应的可读事件注册到 epoll 实例中
                    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
                        error(1, errno, "epoll_ctl add connection fd failed");
                    }
                }
                continue;
            } else {
                // 读写套接字
                socket_fd = events[i].data.fd;
                printf("get event on socket fd == %d \n", socket_fd);
                while (1) {
                    char buf[512];
                    if ((n = read(socket_fd, buf, sizeof(buf))) < 0) {
                        if (errno != EAGAIN) {
                            error(1, errno, "read error");
                            close(socket_fd);
                        }
                        break;
                    } else if (n == 0) {
                        close(socket_fd);
                        break;
                    } else {
                        for (i = 0; i < n; ++i) {
                            buf[i] = rot13_char(buf[i]);
                        }
                        if (write(socket_fd, buf, n) < 0) {
                            error(1, errno, "write error");
                        }
                    }
                }
            }
        }
    }

    free(events);
    close(listen_fd);
}
