#ifndef __MR_MASTER_H__
#define __MR_MASTER_H__

#include <pthread.h>

// 中间键值列表对单元
typedef struct MR_key_values {
    char *key;
    char **values;
    int values_size;
    int values_capacity;
    int values_iter;
    pthread_mutex_t mutex;
} MR_key_values;

#define MR_KVS_KEY(kvs) (kvs).key
#define MR_KVS_VALUES(kvs) (kvs).values
#define MR_KVS_VALUE_AT(kvs, n) (kvs).values[(n)]
#define MR_KVS_LAST_VALUE(kvs) (kvs).values[(kvs).values_size - 1]

// 分区
// 接收map的输出
// 提供reduce的输入
typedef struct MR_partition {
    pthread_mutex_t mutex;          // 共享区互斥量
    MR_key_values *kvs_groups;
    int groups_capacity;
    int num_kvs_groups;
} MR_partition;

#define MR_PARTITION_LAST_KVS_PAIR(partition) (partition).kvs_groups[(partition).num_kvs_groups - 1]
#define MR_PARTITION_KVS_PAIRS(partition) (partition).kvs_groups
#define MR_PARTITION_KVS_PAIR_AT(partition, n) (partition).kvs_groups[(n)]
#define MR_PARTITION_KVS_KEY_AT(partition, n) MR_KVS_KEY((partition).kvs_groups[(n)])
#define MR_PARTITION_KVS_VALUES_AT(partition, n) MR_KVS_VALUES((partition).kvs_groups[(n)])

// 中心数据结构
// map >==> reduce
typedef struct MR_master {
    MR_partition *partitions;       // 分区数组
    int num_partitions;             // 分区数量
} MR_master;

void MR_key_values_init(MR_key_values *kvs, const char *key, int values_capacity);

void MR_key_values_destroy(MR_key_values *kvs);

void MR_key_values_push_value(MR_key_values *kvs, const char *value);

MR_key_values* MR_partition_has_key(MR_partition *partition, const char *key);

void MR_partition_init(MR_partition *partition, int groups_capacity);

void MR_partition_push(MR_partition *partition, const char *key, const char *value);

void MR_partition_destroy(MR_partition *partition);

MR_master *MR_master_create(int num_partitions, int num_pairs);

void MR_master_destroy(MR_master *master);

void quick_kv_sort(MR_key_values *pairs, int low, int high);

#endif