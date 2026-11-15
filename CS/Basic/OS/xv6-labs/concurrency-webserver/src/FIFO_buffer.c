#include "FIFO_buffer.h"

#include <assert.h>
#include <stdlib.h>

#define FIFO_BUFFER_QUEUE_HEAD(buffer) ((buffer)->conn_fds[(buffer)->head])
#define FIFO_BUFFER_QUEUE_TAIL(buffer) ((buffer)->conn_fds[(buffer)->tail])

/* 分配 连接描述符缓冲 */
static int *malloc_conn_fds(int capacity)
{
    int *conn_fds = malloc(sizeof(int) * capacity);
    return conn_fds;
}

/* 初始化 buffer 结构 */
static void init_FIFO_buffer_mem(FIFO_buffer_t *buffer, int capacity)
{
    buffer->capacity = capacity;
    buffer->conn_fds = malloc_conn_fds(capacity);
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->cond_space_available, NULL);
    pthread_cond_init(&buffer->cond_data_available, NULL);
}

/* 分配 FIFO_buffer_t 空间 */
FIFO_buffer_t *FIFO_buffer_create(int capacity)
{
    FIFO_buffer_t *buffer = (FIFO_buffer_t *)malloc(sizeof(FIFO_buffer_t));
    if (buffer == NULL) {
        return NULL;
    }

    init_FIFO_buffer_mem(buffer, capacity);
    return buffer;
}

void FIFO_buffer_destroy(FIFO_buffer_t *buffer)
{
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->cond_space_available);
    pthread_cond_destroy(&buffer->cond_data_available);

    free(buffer->conn_fds);
    free(buffer);
}

/* 需要外部锁保护 */
int FIFO_buffer_count(FIFO_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count;
}

/* 需要外部锁保护 */
int FIFO_buffer_empty(FIFO_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count == 0;
}

/* 需要外部锁保护  */
int FIFO_buffer_full(FIFO_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count == buffer->capacity;
}

int FIFO_buffer_push(int conn_fd, FIFO_buffer_t *buffer)
{
    /* 生产者：生产连接符 */
    pthread_mutex_lock(&buffer->mutex);
    while (FIFO_buffer_full(buffer))
    {
        pthread_cond_wait(&buffer->cond_space_available, &buffer->mutex);
    }
    FIFO_BUFFER_QUEUE_TAIL(buffer) = conn_fd;
    ++(buffer->tail);
    buffer->tail = buffer->tail % buffer->capacity;
    ++buffer->count;

    pthread_cond_signal(&buffer->cond_data_available);
    pthread_mutex_unlock(&buffer->mutex);
    return conn_fd;
}

int FIFO_buffer_pop(FIFO_buffer_t *buffer)
{
    /* 消费者：消费连接符 */
    pthread_mutex_lock(&buffer->mutex);
    while (FIFO_buffer_empty(buffer))
    {
        pthread_cond_wait(&buffer->cond_data_available, &buffer->mutex);
    }
    int conn_fd = FIFO_BUFFER_QUEUE_HEAD(buffer);
    ++(buffer->head);
    buffer->head = buffer->head % buffer->capacity;
    --buffer->count;

    pthread_cond_signal(&buffer->cond_space_available);
    pthread_mutex_unlock(&buffer->mutex);
    return conn_fd;
}
