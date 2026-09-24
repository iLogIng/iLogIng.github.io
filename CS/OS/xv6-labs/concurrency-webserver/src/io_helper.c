#include "io_helper.h"

/* 读取一行数据 */
ssize_t readline(int fd, void *buf, size_t maxlen) {
    char c;
    char *bufp = buf;
    int n;
    for (n = 0; n < maxlen - 1; n++) { // leave room at end for '\0'
        int rc;
        if ((rc = read_or_die(fd, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break;    /* EOF, some data was read */
        } else
            return -1;    /* error */
    }
    *bufp = '\0';
    return n;
}


int open_client_fd(char *hostname, int port) {
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in server_addr;
    
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; 
    
    // Fill in the server's IP address and port 
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; // check h_errno for cause of error 
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *) hp->h_addr, 
          (char *) &server_addr.sin_addr.s_addr, hp->h_length);
    server_addr.sin_port = htons(port);
    
    // Establish a connection with the server 
    if (connect(client_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0)
        return -1;
    return client_fd;
}

int open_listen_fd(int port) {
    // Create a socket descriptor 
    // 创建一个套接字描述符
    int listen_fd;
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }
    
    // Eliminates "Address already in use" error from bind
    // 消除bind()函数调用时 "Address already in use" 错误
    // SOL_SOCKET: 选项层级，表示套接字级别的选项
    // SO_REUSEADDR: 选项名称，表示允许重用本地地址
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt() failed\n");
        return -1;
    }
    
    // Listen_fd will be an endpoint for all requests to port on any IP address for this host
    // 监听套接字将成为所有请求的端点，监听主机上任何IP地址的指定端口
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET; 
    /* htonl htons 将主机字节序转化为网络字节序(大端序) */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听主机上任何IP地址的指定接口
    server_addr.sin_port = htons((unsigned short) port); // 指定端口号
    /* 将套接字绑定到服务器指定的地址和端口 */
    if (bind(listen_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "bind() failed\n");
        return -1;
    }
    
    // Make it a listening socket ready to accept connection requests 
    // 让套接字成为一个监听套接字，随时准备监听连接请求
    // listen函数将该套接字标记为被动套接字，接收传入的连接请求，并指定连接请求队列的最大长度为1024
    if (listen(listen_fd, 1024) < 0) {
        fprintf(stderr, "listen() failed\n");
        return -1;
    }
    return listen_fd;
}


