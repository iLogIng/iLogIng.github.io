#include "scheduler.h"

#include "io_helper.h"
#include "request.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAXBUF (8192)

/* 创建调度缓冲 */
scheduler_t *scheduler_create(int capacity, const char *schedalg)
{
    scheduler_t *scheduler = malloc(sizeof(scheduler_t));
    if (scheduler == NULL) {
        return NULL;
    }
    /* 设置调度策略 */
    if (strcmp(schedalg, "FIFO") == 0) {
        SET_FIFO_SCHED(scheduler);
    } else if (strcmp(schedalg, "SFF") == 0) {
        SET_SFF_SCHED(scheduler);
    } else {
        free(scheduler);
        return NULL;
    }
    /* 设置调度缓冲 */
    IF_FIFO_SCHED(scheduler) {
        scheduler->sched_buffer = FIFO_buffer_create(capacity);
    } else IF_SFF_SCHED(scheduler) {
        scheduler->sched_buffer = SFF_buffer_create(capacity);
    } else {
        free(scheduler);
        return NULL;
    }
    if (scheduler->sched_buffer == NULL) {
        free(scheduler);
        return NULL;
    }
    return scheduler;
}

/* 销毁调度器 */
void scheduler_destroy(scheduler_t *scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    IF_FIFO_SCHED(scheduler) {
        FIFO_buffer_destroy(scheduler->sched_buffer);
    } else IF_SFF_SCHED(scheduler) {
        SFF_buffer_destroy(scheduler->sched_buffer);
    } else {
        free(scheduler->sched_buffer);
    }
    free(scheduler);
}

/* 将连接描述符压入调度器 */
void scheduler_push(int conn_fd, scheduler_t *scheduler)
{
    IF_FIFO_SCHED(scheduler) {
        FIFO_buffer_push(conn_fd, FIFO_SCHED_BUFFER(scheduler));
    } else IF_SFF_SCHED(scheduler) {
        // 在此处，处理文件大小
        char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
        char filename[MAXBUF];

        SFF_buffer_elem_t elem;
        elem.conn_fd = conn_fd;
        elem.file_size = INT32_MAX;

        readline(conn_fd, buf, MAXBUF);
        strcpy(elem.pre_line_buf, buf);
        sscanf(buf, "%s %s %s", method, uri, version);
        int is_static = !strstr(uri, "cgi");
        if (is_static) {
            sprintf(filename, ".%s", uri);
            if (uri[strlen(uri) - 1] == '/') {
                strcat(filename, "index.html");
            }
            struct stat st;
            stat(filename, &st);
            elem.file_size = st.st_size;
        }

        SFF_buffer_push( elem, SFF_SCHED_BUFFER(scheduler));
    }
}

/* 从调度器中取出连接描述符 */
int scheduler_pop(scheduler_t *scheduler, char *pre_line_buf)
{
    int conn_fd;
    IF_FIFO_SCHED(scheduler) {
        conn_fd = FIFO_buffer_pop(FIFO_SCHED_BUFFER(scheduler));
    } else IF_SFF_SCHED(scheduler) {
        conn_fd = SFF_buffer_pop(SFF_SCHED_BUFFER(scheduler), pre_line_buf);
    }
    return conn_fd;
}
