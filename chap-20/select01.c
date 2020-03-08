#include "lib/common.h"

#define  MAXLINE     1024

/**
 * IO 事件的类型
 * 1. 标准输入文件描述符准备好可以读 
 * 2. 监听套接字准备好，新的连接已经建立成功
 * 3. 已连接套接字准备好可以写
 * 4. 如果一个 I/O 事件等待超过了 10 秒，发生了超时事件
 * 
 * 
 * struct timeval {
 *  long   tv_sec;  seconds 
 *  long   tv_usec; // microseconds 
 *  };
 *  - NULL: 表示如果没有 I/O 事件发生，则 select 一直等待下去
 *  - 非零: 等待固定的一段时间后从 select 阻塞调用中返回
 *  - tv_sec tv_usec 都设置成 0， 表示根本不等待，检测完毕立即返回 (较少)
 * 
 * 
 * select 总结 
 * - 读 (内核通知我们套接字可以往里写了，使用 write 函数就不会阻塞)
 * - 写 (内核通知我们套接字可以往里写了，使用 write 函数就不会阻塞)
 *    - 缓存区大: write 不阻塞, 直接返回
 *    - 连接写半边已经关闭, 如果继续写操作将会产生  SIGPIPE 信号
 *    - 套接字上有错误待处理, 使用 write 函数去执行读操作，不阻塞，且返回 -1
 * 
 * 总结
 * - 描述符基数是当前最大描述符 +1
 * - 每次 select 调用完成之后，记得要重置待测试集合
 * 
 * 缺点
 *  select 的默认最大值为  1024
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        error(1, 0, "usage: select01 <IPaddress>");
    }
    int socket_fd = tcp_client(argv[1], SERV_PORT);

    char recv_line[MAXLINE], send_line[MAXLINE];
    int n;

    fd_set readmask;
    fd_set allreads;
    // (初始化了一个描述符集合, 这个描述符集合是空的)
    FD_ZERO(&allreads);
    // 把 0 标准输入设置为待检测
    FD_SET(0, &allreads);
    // 把 socket_fd 标准输入设置为待检测
    FD_SET(socket_fd, &allreads);

    for (;;) {
        // 备份 allreads (在检测到事件后, 原始的集合会被修改)
        readmask = allreads;
        // 通知内核进程, 当一个或者多个 I/O 时间发生后， 控制权返还给应用程序, 由应用程序进行 I/O 事件处理
        int rc = select(socket_fd + 1, &readmask, NULL, NULL, NULL);

        if (rc <= 0) {
            error(1, errno, "select failed");
        }

        // 如果 socket_fd 有读取事件
        if (FD_ISSET(socket_fd, &readmask)) {
            n = read(socket_fd, recv_line, MAXLINE);
            if (n < 0) {
                error(1, errno, "read error");
            } else if (n == 0) {
                error(1, 0, "server terminated \n");
            }
            recv_line[n] = 0;
            fputs(recv_line, stdout);
            fputs("\n", stdout);
        }

        // 如果 0 有读取事件
        if (FD_ISSET(STDIN_FILENO, &readmask)) {
            if (fgets(send_line, MAXLINE, stdin) != NULL) {
                int i = strlen(send_line);
                if (send_line[i - 1] == '\n') {
                    send_line[i - 1] = 0;
                }

                printf("now sending %s\n", send_line);
                ssize_t rt = write(socket_fd, send_line, strlen(send_line));
                if (rt < 0) {
                    error(1, errno, "write failed ");
                }
                printf("send bytes: %zu \n", rt);
            }
        }
    }

}


