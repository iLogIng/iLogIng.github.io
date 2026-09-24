#include "thread_pool.h"

#include <stdlib.h>

static task_node_t *thread_pool_task_node_create(void *(*task)(void *), void *args)
{
    if (task == NULL) {
        return NULL;
    }
    task_node_t *node = malloc(sizeof(task_node_t));
    if (node == NULL) {
        return NULL;
    }

    node->next = NULL;
    node->task = task;
    node->args = args;
    return node;
}

/* 将任务入队 */
static void thread_pool_push_taskq(task_node_t *task_node, thread_pool_t *pool)
{
    if (task_node == NULL || pool == NULL) {
        return;
    }

    if (pool->tail == NULL) {
        pool->tail = pool->head = task_node;
    }
    else {
        pool->tail->next = task_node;
        pool->tail = task_node;
    }
}

/* 将任务出队 */
static task_node_t *thread_pool_pop_taskq(thread_pool_t *pool)
{
    if (pool == NULL) {
        return NULL;
    }

    task_node_t *task = pool->head;
    pool->head = task->next;
    if (pool->head == NULL) {
        pool->tail = NULL;
    }
    return task;
}

/* 任务工作者 */
static void *task_worker(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *) arg;

    while (1)
    {
        pthread_mutex_lock(&pool->mutex);
        /* 无任务，池未关闭 */
        while (pool->head == NULL && !pool->shutdown)
        {
            /* 得到至少一份任务 pool->head != NULL */
            pthread_cond_wait(&pool->cond_taskq, &pool->mutex);
        }
        /* 池已关闭，无任务 */
        if (pool->shutdown && pool->head == NULL) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* 任务出队 */
        task_node_t *task = thread_pool_pop_taskq(pool);
        pthread_mutex_unlock(&pool->mutex);

        if (task != NULL) {
            task->task(task->args);
        }
        free(task);
    }
    
    return NULL;
}

static int thread_pool_init(int num_threads, thread_pool_t *pool)
{
    pool->num_threads = num_threads;
    pool->head = pool->tail = NULL;
    pool->shutdown = 0;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond_taskq, NULL);

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        return -1;
    }
    return 0;
}

thread_pool_t *thread_pool_create(int num_threads)
{
    if (num_threads == 0) {
        return NULL;
    }    

    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (pool == NULL) {
        return NULL;
    }

    if (thread_pool_init(num_threads, pool) != 0) {
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads; ++i)
    {
        if (pthread_create(&pool->threads[i], NULL, task_worker, (void *)pool) != 0) {
            pool->num_threads = i;
            thread_pool_join_all(pool);

            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond_taskq);

            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

/* 为 thread_pool 添加任务 */
int thread_pool_add_task(void *(*task)(void *), void *args, thread_pool_t *pool)
{
    if (pool == NULL || task == NULL) {
        return -1;
    }

    task_node_t *task_node = thread_pool_task_node_create(task, args);
    if (task_node == NULL) {
        return -1;
    }

    pthread_mutex_lock(&pool->mutex);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        free(task_node);
        return -1;
    }

    thread_pool_push_taskq(task_node, pool);

    /* 唤醒一个等待线程 */
    pthread_cond_signal(&pool->cond_taskq);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

/* 释放 thread_pool 的空间 */
void thread_pool_destroy(thread_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }
    
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond_taskq);
    pthread_mutex_unlock(&pool->mutex);

    thread_pool_join_all(pool);

    free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond_taskq);
    free(pool);
}

/* 将 thread_pool 中所有线程合并 */
void thread_pool_join_all(thread_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond_taskq);

    for (int i = 0; i < pool->num_threads; ++i) {
        pthread_join(pool->threads[i], NULL);
    }
}
