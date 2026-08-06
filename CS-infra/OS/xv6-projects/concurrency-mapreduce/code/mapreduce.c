#include "mapreduce.h"

#include <stdlib.h>
#include <sys/sysinfo.h>
#include "thread_pool.h"
#include "MR_master.h"
#include <string.h>

static MR_master *master_struct;
static Mapper global_map;
static Reducer global_reduce;
static Partitioner global_partition;

// MapReduce 默认哈希分区
unsigned long MR_DefaultHashPartition(char *key, int num_partitions)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

// MapReduce 键值对发射
void MR_Emit(char *key, char *value)
{
    unsigned long idx = global_partition(key, master_struct->num_partitions);
    MR_partition_push(&master_struct->partitions[idx], key, value);
}

typedef struct {
    char *file_name;
} map_task_worker_arg;
static void *map_task_worker(void *arg)
{
    map_task_worker_arg *map_arg = (map_task_worker_arg *)arg;
    char *file_name = map_arg->file_name;
    global_map(file_name);
    return NULL;
}

typedef struct {
    char *key;
    Getter get_func;
    int partition_number;
}reduce_task_worker_arg;
static void *reduce_task_worker(void *arg)
{
    reduce_task_worker_arg *reduce_arg = (reduce_task_worker_arg *)arg;
    char *key = reduce_arg->key;
    Getter get_func = reduce_arg->get_func;
    int partition_number = reduce_arg->partition_number;
    global_reduce(key, get_func, partition_number);
    return NULL;
}

// 区内迭代器，获取值
static char *MR_partition_iter_get(char *key, int partition_number)
{
    MR_partition *partition = &master_struct->partitions[partition_number];
    MR_key_values *kvs = MR_partition_has_key(partition, key);
    if (kvs == NULL) {
        return NULL;
    }
    char *value = NULL;
    pthread_mutex_lock(&kvs->mutex);
    if (kvs->values_iter >= kvs->values_size) {
        pthread_mutex_unlock(&kvs->mutex);
        return NULL;
    }
    value = MR_KVS_VALUE_AT(*kvs, kvs->values_iter);
    ++kvs->values_iter;
    pthread_mutex_unlock(&kvs->mutex);
    return value;
}

/**
 * MapReduce 运行
 * 接收:
 * - 给定程序的命令行参数
 * - 指向Map函数的指针
 * - MapReduce库创建的线程数量
 * - 指向Reduce函数的指针
 * - reducer数量
 * - 指向分区函数的指针
*/
void MR_Run(int argc, char *argv[],
	    Mapper map, int num_mappers,
	    Reducer reduce, int num_reducers,
	    Partitioner partition)
{
    // 初始化全局状态
    int num_partitions = argc - 1;
    master_struct = MR_master_create(num_partitions, 20);
    global_map = map;
    global_reduce = reduce;
    global_partition = partition;

    // map reduce线程池
    thread_pool_t *map_pool = thread_pool_create(num_mappers);
    thread_pool_t *reduce_pool = thread_pool_create(num_reducers);

    // 启动 map 任务
    int file_count = argc - 1;
    map_task_worker_arg *maps_arg = malloc(sizeof(map_task_worker_arg) * file_count);
    for (int i = 0; i < argc - 1; ++i) {
        maps_arg[i].file_name = argv[i + 1];
        thread_pool_add_task(map_task_worker, &maps_arg[i], map_pool);
    }
    thread_pool_destroy(map_pool);

    // 快速排序
    MR_partition *partitions = master_struct->partitions;
    for (int i = 0; i < master_struct->num_partitions; ++i)
    {
        quick_kv_sort(partitions[i].kvs_groups, 0, partitions[i].num_kvs_groups - 1);
    }

    int count_kv_pair = 0;
    for (int i = 0; i < master_struct->num_partitions; ++i) {
        count_kv_pair += master_struct->partitions[i].num_kvs_groups;
    }
    reduce_task_worker_arg *reduces_arg = malloc(sizeof(reduce_task_worker_arg) * count_kv_pair);

    int idx = 0;
    // 启动 reduce 任务
    for (int i = 0; i < master_struct->num_partitions; ++i) {
        MR_partition *cur_partition = &master_struct->partitions[i];
        for (int j = 0; j < cur_partition->num_kvs_groups; ++j) {
            reduces_arg[idx] = (reduce_task_worker_arg) {
                .get_func = MR_partition_iter_get,
                .key = cur_partition->kvs_groups[j].key,
                .partition_number = i
            };
            thread_pool_add_task(reduce_task_worker, &reduces_arg[idx], reduce_pool);
            ++idx;
        }
    }

    thread_pool_destroy(reduce_pool);
    MR_master_destroy(master_struct);
    free(maps_arg);
    free(reduces_arg);
}

