#include "lib/common.h"

#define INIT_SIZE 128
/*
 * timeout
 *  - <0  表示在有事件发生之前永远等待
 *  - ==0 表示不阻塞进程，立即返回
 *  - >0  poll 调用方等待指定的毫秒数后返回
 * 
 * return value
 *  - -1 错误
 *  - 0 在指定时间内没有任何时间发生
 *  - 非零 返回时间发生的个数 (也就是“returned events”中非 0 的描述符个数)
 * 
 * 不想对某个 pollfd 结构进行事件检测 pollfd.fd 设置为负数
 * int poll(struct pollfd fds[], nfds_t nfds, int timeout);
 * 
 * struct pollfd {
 *  int    fd;       // file descriptor 
 *  short  events;   // events to look for 待检测事件
 *  short  revents;  // events returned  发生的事件 
 * }; 
 * 
 * 可读
 * #define POLLIN     0x0001     any readable data available 一般使用
 * #define POLLPRI    0x0002     OOB/Urgent readable data
 * #define POLLRDNORM 0x0040     non-OOB/URG data available
 * #define POLLRDBAND 0x0080     OOB/Urgent readable data
 * 
 * 可写 
 * #define POLLOUT    0x0004     file descriptor is writeable  一般使用
 * #define POLLWRNORM POLLOUT    no write type differentiation
 * #define POLLWRBAND 0x0100     OOB/Urgent data can be written 
 * 
 * 读写可以在 "returned events" 复用
 * 
 * 错误 (没有办法通过 poll 向系统内核递交检测请求，只能通过“returned events”来加以检测)
 * #define POLLERR    0x0008     一些错误发送 
 * #define POLLHUP    0x0010     描述符挂起 
 * #define POLLNVAL   0x0020     请求的事件无效 
 *
 */
int main(int argc, char **argv) {
    int listen_fd, connected_fd;
    int ready_number;
    ssize_t n;
    char buf[MAXLINE];
    struct sockaddr_in client_addr;

    listen_fd = tcp_server_listen(SERV_PORT);

    //初始化pollfd数组，这个数组的第一个元素是listen_fd，其余的用来记录将要连接的connect_fd
    struct pollfd event_set[INIT_SIZE];
    event_set[0].fd = listen_fd;
    event_set[0].events = POLLRDNORM;

    // 用-1表示这个数组位置还没有被占用
    int i;
    for (i = 1; i < INIT_SIZE; i++) {
        event_set[i].fd = -1;
    }

    for (;;) {
        // 阻塞等待 ready_number 的个数
        if ((ready_number = poll(event_set, INIT_SIZE, -1)) < 0) {
            error(1, errno, "poll failed ");
        }

        // 连接事件， POLLRDNORM 套接字有数据可以读取
        if (event_set[0].revents & POLLRDNORM) {
            socklen_t client_len = sizeof(client_addr);
            // accept 获取连接描述
            connected_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);

            //找到一个可以记录该连接套接字的位置
            for (i = 1; i < INIT_SIZE; i++) {
                if (event_set[i].fd < 0) {
                    event_set[i].fd = connected_fd;
                    event_set[i].events = POLLRDNORM;
                    break;
                }
            }

            // 连接的位置不够了, 满了
            if (i == INIT_SIZE) {
                error(1, errno, "can not hold so many clients");
            }

            // 这个 poll 的描述符事件已经处理完毕， 就直接进入 poll 函数, 继续监听事件
            if (--ready_number <= 0)
                continue;
        }

        // event_set 其它事件(套接字的可读事件)
        for (i = 1; i < INIT_SIZE; i++) {
            int socket_fd;
            if ((socket_fd = event_set[i].fd) < 0)
                continue;
            // 可读或者发生错误
            if (event_set[i].revents & (POLLRDNORM | POLLERR)) {
                // 正常读取, 发送回客户端
                if ((n = read(socket_fd, buf, MAXLINE)) > 0) {
                    if (write(socket_fd, buf, n) < 0) {
                        error(1, errno, "write error");
                    }
                } else if (n == 0 || errno == ECONNRESET) {
                    // 如果读到 EOF 或者连接重置, 则关闭这个连接,  并且把 event_set 对应的 pollfd 重置
                    close(socket_fd);
                    event_set[i].fd = -1;
                } else {
                    // 读取数据失败
                    error(1, errno, "read error");
                }

                // 如果事件被处理完, break
                if (--ready_number <= 0)
                    break;
            }
        }
    }
}
