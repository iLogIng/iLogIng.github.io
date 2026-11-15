#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>

#include "request.h"
#include "io_helper.h"

#include "scheduler.h"
#include "thread_pool.h"

#define MAXBUF (8192)

char default_root[] = ".";
char default_schedalg[] = "FIFO";

/* buffer scheduler */
scheduler_t *scheduler;

/* request_thread */
void *request_thread(void *args);

//
// ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
// 
int main(int argc, char *argv[]) {
    int c;

	/* 服务器根目录 */
    char *root_dir = default_root;
	/* 服务器端口 */
    int port = 10000;
	/* 线程并发数 */
	int threads = 1;
	/* 缓冲区大小 */
	int buffers = 0x800;
	/* 调度策略 FIFO or SFF */
	char *schedalg = default_schedalg;

	/* 使用了 optarg 变量，在 <unistd.h>中定义并在getopt中使用 */
    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
	{
		switch (c) {
		case 'd':
			root_dir = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			threads = atoi(optarg);
			break;
		case 'b':
			buffers = atoi(optarg);
			break;
		case 's':
			schedalg = optarg;
			break;
		default:
			fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]\n");
			exit(1);
		}
	}

	/* 初始化 conn_buffer */
	scheduler = scheduler_create(buffers, schedalg);

	// 切换当前工作目录到指定的根目录
    chdir_or_die(root_dir);

	/* 多线程并发 */
	thread_pool_t *thrd_pool = thread_pool_create(threads);
	for (int i = 0; i < threads; ++i) {
		thread_pool_add_task(request_thread, NULL, thrd_pool);
	}

    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
		/* 客户端的套接字地址 */
		struct sockaddr_in client_addr;
		/* 获取客户端的地址长度 */
		int client_len = sizeof(client_addr);
		/* 调用 accept函数，从监听套接字处 接收一个连接 */
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		scheduler_push(conn_fd, scheduler);
    }

	thread_pool_join_all(thrd_pool);

	scheduler_destroy(scheduler);
	thread_pool_destroy(thrd_pool);
    return 0;
}

void *request_thread(void *args)
{
	char buf[MAXBUF];
	while (1)
	{
		int conn_fd = scheduler_pop(scheduler, buf);
		IF_SFF_SCHED(scheduler) {
			request_handle(conn_fd, buf);
		} else {
			request_handle(conn_fd, NULL);
		}
		close_or_die(conn_fd);
	}
	return NULL;
}
