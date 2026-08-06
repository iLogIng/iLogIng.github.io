#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "psort.h"
#include "thread_pool.h"

// 排序任务
void* sort_worker(void *arg);

// 输入文件内存映射
mfilemap input;
// 输出文件内存映射
mfilemap output;

// 最小堆，用于 k 路归并
typedef struct {
    RecordKey key;
    int group_idx;
} heap_node_t;

static void heap_sift_down(heap_node_t *heap, int size, int i);

static void heap_sift_up(heap_node_t *heap, int i);

static void heap_push(heap_node_t *heap, int *size, RecordKey key, int group_idx);

static heap_node_t heap_pop(heap_node_t *heap, int *size);

int main(int argc, char *argv[])
{
#if 0
    // 产生的 RecordKey 的基值
    const RecordKey base = 0x00;
    // 产生的 records 条数
    const int records = 100000; // 产生 100'000 * 100 ~ 10MB
    // 随机 records key范围
    const float multiply = 2.5f;
    create_random_Record_file("test.dat", base, records, multiply, time(NULL));
#else

    if (argc != 3) {
        fprintf(stderr, "./psort must have two args\n\t./psort [input] [output]\n");
        exit(1);
    }
    const char *finput = argv[1];
    const char *foutput = argv[2];
    printf("input:%s\noutput:%s\n", finput, foutput);

    // 线程池大小与初始化
    int threads = get_nprocs();
    thread_pool_t *thread_pool = thread_pool_create(threads);
    
    input = mfilemap_read_open(finput, 0, PROT_READ | PROT_NONE, MAP_PRIVATE);
    // fprintf(stdout, "size: %ld\n", input.size);

    int num_records = input.size / RECORD_SIZE;
    int num_per_group = (num_records + threads) / threads;
    // fprintf(stdout, "num_records:%d\n", num_records);
    // fprintf(stdout, "threads:%d\n", threads);
    // fprintf(stdout, "num_per_group:%d\n", num_per_group);

    // 分配各组参数
    sort_group *groups = (sort_group *)malloc(sizeof(sort_group) * threads);
    for (int i = 0; i < threads; ++i) {
        int group_records = ((i != threads - 1)? num_per_group : num_records - i * num_per_group);
        sort_group_init(&groups[i], group_records);
        Record *rbase = (Record *)input.mbuf + i * num_per_group;
        for (int j = 0; j < groups[i].num_records; ++j) {
            groups[i].list[j] = ((Record *)rbase + j);
        }
    }

    for (int i = 0; i < threads; ++i) {
        thread_pool_add_task(sort_worker, &groups[i], thread_pool);
    }
    
    thread_pool_destroy(thread_pool);

    output = mfilemap_write_open(foutput, input.size, 0, PROT_WRITE | PROT_NONE, MAP_SHARED);

    // 构建初始最小堆，每个非空组的第一条记录入堆
    heap_node_t *heap = (heap_node_t *)malloc(sizeof(heap_node_t) * threads);
    int heap_size = 0;
    for (int i = 0; i < threads; ++i) {
        if (groups[i].num_records > 0) {
            heap_push(heap, &heap_size, groups[i].list[0]->fields.key, i);
        }
    }

    // k 路归并，每次从堆中弹出最小记录，并从同组补入下一条
    Record *out_records = (Record *)output.mbuf;
    int out_idx = 0;
    while (heap_size > 0) {
        heap_node_t min = heap_pop(heap, &heap_size);
        sort_group *g = &groups[min.group_idx];
        memcpy(&out_records[out_idx++], g->list[g->iter], RECORD_SIZE);
        g->iter++;
        if (g->iter < g->num_records) {
            heap_push(heap, &heap_size,
                      g->list[g->iter]->fields.key, min.group_idx);
        }
    }
    free(heap);

    for (int i = 0; i < threads; ++i) {
        free(groups[i].list);
    }
    free(groups);
    mfilemap_close(input);

    fsync(output.fd);
    mfilemap_close(output);

    check_Record_file_sorted(foutput);
#endif
    return 0;
}

void* sort_worker(void *arg)
{
    sort_group *group = (sort_group *)arg;

    int num_records = group->num_records;
    Record **list = group->list;

    group_qsort(list, 0, num_records - 1);
    
    return NULL;
}

void heap_sift_down(heap_node_t *heap, int size, int i) {
    while (1) {
        int smallest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < size && heap[left].key < heap[smallest].key) {
            smallest = left;
        }
        if (right < size && heap[right].key < heap[smallest].key) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        heap_node_t tmp = heap[i];
        heap[i] = heap[smallest];
        heap[smallest] = tmp;
        i = smallest;
    }
}

void heap_sift_up(heap_node_t *heap, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[i].key >= heap[parent].key) {
            break;
        }
        heap_node_t tmp = heap[i];
        heap[i] = heap[parent];
        heap[parent] = tmp;
        i = parent;
    }
}

void heap_push(heap_node_t *heap, int *size, RecordKey key, int group_idx) {
    heap[*size].key = key;
    heap[*size].group_idx = group_idx;
    heap_sift_up(heap, *size);
    (*size)++;
}

heap_node_t heap_pop(heap_node_t *heap, int *size) {
    heap_node_t top = heap[0];
    (*size)--;
    heap[0] = heap[*size];
    heap_sift_down(heap, *size, 0);
    return top;
}