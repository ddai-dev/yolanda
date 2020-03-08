# include "common.h"

int tcp_client(char *address, int port) {
    int socket_fd;

    // domain Pv4还是IPv6这个范围内通信
    // type 类型: tcp udp raw
    //  protocol 废弃
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    // The inet_pton() function converts a presentation format address
    // (that is, printable form as held in a character string) to network format
    inet_pton(AF_INET, address, &server_addr.sin_addr);

    socklen_t server_len = sizeof(server_addr);
    // connect(int socket, const struct sockaddr *address, socklen_t address_len); 强转
    int connect_rt = connect(socket_fd, (struct sockaddr *) &server_addr, server_len);
    if (connect_rt < 0) {
        error(1, errno, "connect failed ");
    }

    return socket_fd;
}