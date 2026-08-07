#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "FIFO_buffer.h"
#include "SFF_buffer.h"

/* NULL     调度*/
#define SCHED_ALG_NULL      0x00
/* FIFO     调度 */
#define SCHED_ALG_FIFO      0x01
/* SFF      调度 */
#define SCHED_ALG_SFF       0x02

#define SET_FIFO_SCHED(scheduler) ((scheduler)->sched_alg = SCHED_ALG_FIFO)
#define SET_SFF_SCHED(scheduler) ((scheduler)->sched_alg = SCHED_ALG_SFF)

#define IS_FIFO_SCHED(scheduler) ((scheduler)->sched_alg == SCHED_ALG_FIFO)
#define IS_SFF_SCHED(scheduler) ((scheduler)->sched_alg == SCHED_ALG_SFF)

#define IF_FIFO_SCHED(scheduler) if ((scheduler)->sched_alg == SCHED_ALG_FIFO)
#define IF_SFF_SCHED(scheduler) if ((scheduler)->sched_alg == SCHED_ALG_SFF)

#define FIFO_SCHED_BUFFER(scheduler) ((FIFO_buffer_t *)(scheduler)->sched_buffer)
#define SFF_SCHED_BUFFER(scheduler) ((SFF_buffer_t *)(scheduler)->sched_buffer)

typedef unsigned int sched_alg_t;

typedef struct {
    sched_alg_t sched_alg;
    void *sched_buffer;
} scheduler_t;

/* 创建调度缓冲 */
scheduler_t *scheduler_create(int capacity, const char *schedalg);

/* 销毁调度器 */
void scheduler_destroy(scheduler_t *scheduler);

/* 将连接符压入调度器 */
void scheduler_push(int conn_fd, scheduler_t *scheduler);

/* 从调度器中取出连接符 */
int scheduler_pop(scheduler_t *scheduler, char *pre_line_buf);

#endif // __SCHEDULER_H__
