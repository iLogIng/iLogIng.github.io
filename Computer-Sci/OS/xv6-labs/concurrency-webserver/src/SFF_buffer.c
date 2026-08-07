#include "SFF_buffer.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static int parent_node(int idx);
static int left_node(int idx);
static int right_node(int idx);

static void shift_up(int idx, SFF_buffer_t *buffer);
static void shift_down(int idx, SFF_buffer_t *buffer);

#define SFF_BUFFER_MIN_ELEM(buffer) ((buffer)->elems[0])
#define SFF_BUFFER_CAPACITY(buffer) ((buffer)->capacity)
#define SFF_BUFFER_ELEM_COUNT(buffer) ((buffer)->count)

/* 初始化 buffer 结构 */
static int init_SFF_buffer_mem(SFF_buffer_t *buffer, int capacity)
{
    buffer->capacity = capacity;
    buffer->elems = (SFF_buffer_elem_t *)malloc(sizeof(SFF_buffer_elem_t) * capacity);
    if (buffer->elems == NULL) {
        return -1;
    }
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->cond_space_available, NULL);
    pthread_cond_init(&buffer->cond_data_available, NULL);
    return 0;
}

SFF_buffer_t *SFF_buffer_create(int capacity)
{
    SFF_buffer_t *buffer = malloc(sizeof(SFF_buffer_t));
    if (buffer == NULL) {
        return NULL;
    }

    if (init_SFF_buffer_mem(buffer, capacity) == -1) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

void SFF_buffer_destroy(SFF_buffer_t *buffer)
{
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->cond_data_available);
    pthread_cond_destroy(&buffer->cond_space_available);

    free(buffer->elems);
    free(buffer);
}

/* 需要外部加锁 */
int SFF_buffer_count(SFF_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count;
}

/* 需要外部加锁 */
int SFF_buffer_empty(SFF_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count == 0;
}

/* 需要外部加锁 */
int SFF_buffer_full(SFF_buffer_t *buffer)
{
    assert(buffer != NULL);
    return buffer->count == buffer->capacity;
}

int SFF_buffer_push(SFF_buffer_elem_t elem, SFF_buffer_t *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    while (SFF_buffer_full(buffer))
    {
        pthread_cond_wait(&buffer->cond_space_available, &buffer->mutex);
    }
    /* insert at end and shift up to maintain max-heap by file_size */
    int i = buffer->count;
    buffer->elems[i] = elem;
    buffer->count++;
    shift_up(i, buffer);

    pthread_cond_signal(&buffer->cond_data_available);
    pthread_mutex_unlock(&buffer->mutex);
    return elem.conn_fd;
}

int SFF_buffer_pop(SFF_buffer_t *buffer, char *pre_line_buf)
{
    pthread_mutex_lock(&buffer->mutex);
    while (SFF_buffer_empty(buffer))
    {
        pthread_cond_wait(&buffer->cond_data_available, &buffer->mutex);
    }
    /* pop max (root) */
    int conn_fd = SFF_BUFFER_MIN_ELEM(buffer).conn_fd;
    strcpy(pre_line_buf, SFF_BUFFER_MIN_ELEM(buffer).pre_line_buf);
    buffer->count--;
    if (buffer->count > 0) {
        buffer->elems[0] = buffer->elems[buffer->count];
        shift_down(0, buffer);
    }
    pthread_cond_signal(&buffer->cond_space_available);
    pthread_mutex_unlock(&buffer->mutex);
    return conn_fd;
}

/* MIN TOP HEAP */

int parent_node(int idx)
{
    return (idx - 1) >> 1;    
}
int left_node(int idx)
{
    return (idx << 1) + 1;
}
int right_node(int idx)
{
    return (idx << 1) + 2;
}

void shift_up(int idx, SFF_buffer_t *buffer)
{
    if (idx == 0) {
        return;
    }

    int parent = parent_node(idx);
    SFF_buffer_elem_t *elems = buffer->elems;
    while (idx > 0
        && elems[idx].file_size > elems[parent].file_size)
    {
        /* 小值子节点上浮 */
        SFF_buffer_elem_t tmp = elems[parent];
        elems[parent] = elems[idx];
        elems[idx] = tmp;
        //
        idx = parent;
        parent = parent_node(idx);
    }
}

void shift_down(int idx, SFF_buffer_t *buffer)
{
    SFF_buffer_elem_t *elems = buffer->elems;
    int count = SFF_BUFFER_ELEM_COUNT(buffer);

    while (1)
    {
        int l = left_node(idx);
        int r = right_node(idx);
        int child = idx;

        /* 检查子节点，是否更小 */
        if (l < count
            && elems[l].file_size > elems[child].file_size)
        {
            child = l;
        }
        if (r < count
            && elems[r].file_size > elems[child].file_size)
        {
            child = r;
        }

        if (child == idx) {
            break;
        }

        SFF_buffer_elem_t tmp = elems[idx];
        elems[idx] = elems[child];
        elems[child] = tmp;
        /* 大值下沉 */
        idx = child;
    }
}
