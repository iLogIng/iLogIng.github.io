#ifndef __SFF_BUFFER_H__
#define __SFF_BUFFER_H__

#include <pthread.h>

#define PRELBUFSIZE (8192)

typedef struct {
    int conn_fd;
    int file_size;
    char pre_line_buf[PRELBUFSIZE];
} SFF_buffer_elem_t;

typedef struct {
    SFF_buffer_elem_t *elems;                   /* 元素列表 */
    int capacity;                               /* 缓冲容量 */
    int count;                                  /* 元素数量 */
    pthread_mutex_t mutex;                      /* 互斥量 */
    pthread_cond_t cond_space_available;        /* 条件变量：缓冲区可写 */
    pthread_cond_t cond_data_available;         /* 条件变量：缓冲区数据可读 */
} SFF_buffer_t;

SFF_buffer_t *SFF_buffer_create(int capacity);

void SFF_buffer_destroy(SFF_buffer_t *buffer);

int SFF_buffer_count(SFF_buffer_t *buffer);

int SFF_buffer_empty(SFF_buffer_t *buffer);

int SFF_buffer_full(SFF_buffer_t *buffer);

int SFF_buffer_push(SFF_buffer_elem_t elem, SFF_buffer_t *buffer);

int SFF_buffer_pop(SFF_buffer_t *buffer, char *pre_line_buf);

#endif