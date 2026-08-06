#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

/* 任务结点 */
typedef struct task_node_t {
    void *(*task)(void *);
    void *args;
    struct task_node_t *next;
} task_node_t;

/* 线程池类型 */
typedef struct {
    pthread_mutex_t mutex;                  /* 互斥量：保护 thread_pool_t 结构内部字段 */
    pthread_cond_t cond_taskq;              /* 条件变量：队列非空，shutdown时唤起 */
    pthread_t *threads;                     /* 工作线程组 */
    int num_threads;                        /* 线程数量 */
    int shutdown;                           /* 0/1 关闭标识 */
    task_node_t *head;                      /* 任务队列，队头 */
    task_node_t *tail;                      /* 任务队列，队尾 */
} thread_pool_t;

/* 为 thread_pool 分配空间 */
thread_pool_t *thread_pool_create(int num_threads);

/* 为 thread_pool 添加任务 */
int thread_pool_add_task(void *(*task)(void *), void *args, thread_pool_t *pool);

/* 释放 thread_pool 的空间 */
void thread_pool_destroy(thread_pool_t *pool);

/* 将 thread_pool 中所有线程合并 */
void thread_pool_join_all(thread_pool_t *pool);

#endif
