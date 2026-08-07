#ifndef __FIFO_BUFFER_H__
#define __FIFO_BUFFER_H__

#include <pthread.h>

typedef struct {
    int *conn_fds;                              /* 连接描述符，环形缓冲队列 */
    int capacity;                               /* 缓冲区大小 */
    int head, tail;                             /* 读写位置 */
    int count;                                  /* 当前元素数 */
    pthread_mutex_t mutex;                      /* 互斥量 */
    pthread_cond_t cond_space_available;        /* 条件变量：缓冲区可写 */
    pthread_cond_t cond_data_available;         /* 条件变量：缓冲区数据可读 */
} FIFO_buffer_t;

FIFO_buffer_t *FIFO_buffer_create(int capacity);

void FIFO_buffer_destroy(FIFO_buffer_t *buffer);

int FIFO_buffer_count(FIFO_buffer_t *buffer);

int FIFO_buffer_empty(FIFO_buffer_t *buffer);

int FIFO_buffer_full(FIFO_buffer_t *buffer);

int FIFO_buffer_push(int conn_fd, FIFO_buffer_t *buffer);

int FIFO_buffer_pop(FIFO_buffer_t *buffer);

#endif // __FIFO_buffer_H__
