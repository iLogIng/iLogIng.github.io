//
// client.c: A very, very primitive HTTP client.
// client.c: 一个非常原始的HTTP客户端
// 
// To run, try: 
//      client hostname portnumber filename
// 运行，并尝试：
//      client 主机名 端口号 文件名
//
// Sends one HTTP request to the specified HTTP server.
// Prints out the HTTP response.
// 发送一个HTTP请求到指定的HTTP服务器。
// 打印出HTTP响应
//
// For testing your server, you will want to modify this client.  
// For example:
// You may want to make this multi-threaded so that you can 
// send many requests simultaneously to the server.
// 为了测试你的服务器，你将要修改该客户端，
// 例如：
// 你可能想让它实现多线程，以便可以同时向服务器发送多个请求
//
// You may also want to be able to request different URIs; 
// you may want to get more URIs from the command line 
// or read the list from a file. 
// 你可能也想请求多个不同的URI
// 你可能想从命令行或文件中获取包含多个URI的列表
//
// When we test your server, we will be using modifications to this client.
// 当我们测试你的服务器时，我们将使用该客户端修改后的版本
//

#include "io_helper.h"

#define MAXBUF (8192)

//
// Send an HTTP request for the specified file 
// 发送一个HTTP请求，获取指定文件
//
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    /* 获取主机名 */
    gethostname_or_die(hostname, MAXBUF);
    
    /* Form and send the HTTP request */
    /* 建立并发送HTTP请求 */
    sprintf(buf, "GET %s HTTP/1.1\n", filename);
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
    write_or_die(fd, buf, strlen(buf));
}

//
// Read the HTTP response and print it out
// 读取并打印HTTP响应
//
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    // Read and display the HTTP Header 
    // 读并显示HTTP请求头
    n = readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
        printf("Header: %s", buf);
        n = readline_or_die(fd, buf, MAXBUF);
        
        // If you want to look for certain HTTP tags... 
        // 你若想寻找特定的HTTP标签...
        // int length = 0;
        //if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
        //    printf("Length = %d\n", length);
        //}
    }
    
    // Read and display the HTTP Body 
    // 读取并显示HTTP响应体
    n = readline_or_die(fd, buf, MAXBUF);
    while (n > 0) {
        printf("%s", buf);
        n = readline_or_die(fd, buf, MAXBUF);
    }
}

int main(int argc, char *argv[]) {
    char *host, *filename;
    int port;
    int clientfd;
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <filename>\n", argv[0]);
        exit(1);
    }
    
    host = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    
    /* Open a single connection to the specified host and port */
    /* 打开一个与特定主机端口的连接 */
    clientfd = open_client_fd_or_die(host, port);
    
    client_send(clientfd, filename);
    client_print(clientfd);
    
    close_or_die(clientfd);
    
    exit(0);
}
