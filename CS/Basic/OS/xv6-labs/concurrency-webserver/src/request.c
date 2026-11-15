#include <pthread.h>

#include "io_helper.h"
#include "request.h"

//
// Some of this code stolen from Bryant/O'Halloran
// Hopefully this is not a problem ... :)
// 一些代码来自 Bryant/O'Halloran 的教材，希望这不是个问题 ... :)
//

#define MAXBUF (8192)

void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXBUF], body[MAXBUF];
    
    // Create the body of error message first (have to know its length for header)
    // 创建错误消息的主体部分（必须知道其长度以便生成HTTP响应头）
    sprintf(body, ""
	    "<!doctype html>\r\n"
	    "<head>\r\n"
	    "  <title>OSTEP WebServer Error</title>\r\n"
	    "</head>\r\n"
	    "<body>\r\n"
	    "  <h2>%s: %s</h2>\r\n" 
	    "  <p>%s: %s</p>\r\n"
	    "</body>\r\n"
	    "</html>\r\n", errnum, shortmsg, longmsg, cause);
    
    // Write out the header information for this response
    // 写出HTTP响应头信息
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    write_or_die(fd, buf, strlen(buf));
    
    // Write out the body last
    // 写出HTTP相应体
    write_or_die(fd, body, strlen(body));
}

//
// Reads and discards everything up to an empty text line
// 读并且丢弃所有文本行直到一个空行(HTTP 请求头的结束标志)
//
void request_read_headers(int fd) {
    char buf[MAXBUF];
    
    readline_or_die(fd, buf, MAXBUF);
    /* \r\n即 HTTP 请求头的结束标志 */
    while (strcmp(buf, "\r\n")) {
        readline_or_die(fd, buf, MAXBUF);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// 若是静态的则返回 1，否则为动态返回 0
// Calculates filename (and cgiargs, for dynamic) from uri
// 从uri中计算文件名 以及 动态内容的 CGI参数
//
int request_parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (strstr(uri, "..") != NULL) {
        return -1;
    }

    if (!strstr(uri, "cgi")) { 
        // static
        strcpy(cgiargs, "");
        sprintf(filename, ".%s", uri);
        if (uri[strlen(uri)-1] == '/') {
            strcat(filename, "index.html");
        }
        return 1;
    } else { 
        // dynamic
        // 动态请求，将CGI参数提取至cgiargs中
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        sprintf(filename, ".%s", uri);
        return 0;
    }
}

//
// Fills in the filetype given the filename
// 根据文件名填充文件类型
//
void request_get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) 
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
        strcpy(filetype, "image/jpeg");
    else 
        strcpy(filetype, "text/plain");
}

void request_serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXBUF], *argv[] = { NULL };
    
    // The server does only a little bit of the header.  
    // The CGI script has to finish writing out the header.
    // 该服务器只写出了一部分HTTP响应头
    // CGI脚本必须完成剩余的HTTP响应头的写出
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n");
    
    write_or_die(fd, buf, strlen(buf));
    
    /* 调起一个子进程执行CGI程序 */
    if (fork_or_die() == 0) {                        // child
        setenv_or_die("QUERY_STRING", cgiargs, 1);   // args to cgi go here
        dup2_or_die(fd, STDOUT_FILENO);              // make cgi writes go to socket (not screen)
        extern char **environ;                       // defined by libc 
        execve_or_die(filename, argv, environ);
    } else {
        wait_or_die(NULL);
    }
}

void request_serve_static(int fd, char *filename, int filesize) {
    /* 源文件的文件描述符 */
    int srcfd;
    /* 源文件指针，文件类型，缓冲区 */
    char *srcp, filetype[MAXBUF], buf[MAXBUF];
    
    /* 获取文件类型并打开文件 */
    request_get_filetype(filename, filetype);
    srcfd = open_or_die(filename, O_RDONLY, 0);
    
    // Rather than call read() to read the file into memory, 
    // which would require that we allocate a buffer, we memory-map the file
    // 这需要我们分配一个缓冲区，我们使用内存映射的方式来访问文件
    // 而非调用read()函数将文件读入内存
    srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close_or_die(srcfd);
    
    // put together response
    // 生成HTTP响应头
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n"
	    "Content-Length: %d\r\n"
	    "Content-Type: %s\r\n\r\n", 
	    filesize, filetype);
    
    write_or_die(fd, buf, strlen(buf));
    
    // Writes out to the client socket the memory-mapped file 
    // 将内存映射文件相应到客户端套接字上
    write_or_die(fd, srcp, filesize);
    munmap_or_die(srcp, filesize); /* 解除一个内存映射 */
}

// handle a request
// 传入一个文件描述符，处理请求
void request_handle(int fd, const char *pre_line_buf) {
    int is_static;
    struct stat sbuf;
    char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
    char filename[MAXBUF], cgiargs[MAXBUF];
    
    /* 读取请求 */
    if (pre_line_buf) { /* SFF */
        strcpy(buf, pre_line_buf);
    } else { /* FIFO */
        readline_or_die(fd, buf, MAXBUF);
    }
    /* 解析请求行 method(命令) uri(唯一资源标识符) version(HTTP协议版本) */
    sscanf(buf, "%s %s %s", method, uri, version);

    /* 回显解析的 method uri version */
    /* uri回显的是基于服务器工作根目录的资源路径 */
    pthread_t tid = pthread_self();
    printf("[thread:%p] method:%s uri:%s version:%s\n", (void *)tid, method, uri, version);
    
    if (strcasecmp(method, "GET")) {
        request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
        return;
    }
    request_read_headers(fd);
    /* 此时 HTTP请求头 已经解析完成 */
    
    /* 解析请求 URI */
    is_static = request_parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        request_error(fd, filename, "404", "Not found", "server could not find this file");
        return;
    }
    
    /* 根据 is_static 判断如何处理请求 */
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            request_error(fd, filename, "403", "Forbidden", "server could not read this file");
            return;
        }
        /* 响应静态请求 */
        request_serve_static(fd, filename, sbuf.st_size);
        } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            request_error(fd, filename, "403", "Forbidden", "server could not run this CGI program");
            return;
        }
        /* 响应动态请求 */
        request_serve_dynamic(fd, filename, cgiargs);
    }
}