#include "MR_master.h"

#include <stdlib.h>
#include <string.h>

void MR_key_values_init(MR_key_values *kvs, const char *key, int values_capacity)
{
    if (kvs == NULL) {
        return;
    }
    // 初始化 key 字段
    if (key == NULL) {
        kvs->key = NULL;
    } else {
        kvs->key = (char *)malloc(sizeof(char) * (strlen(key) + 1));
        strcpy(kvs->key, key);
    }
    // 初始化 values 元素列表 字段
    if (values_capacity <= 0) {
        values_capacity = 1;
    }
    kvs->values = (char **)malloc(sizeof(char *) * values_capacity);
    for (int i = 0; i < values_capacity; ++i) {
        kvs->values[i] = NULL;
    }
    kvs->values_capacity = values_capacity;
    kvs->values_size = 0;
    kvs->values_iter = 0;
    pthread_mutex_init(&kvs->mutex, NULL);
}

void MR_key_values_destroy(MR_key_values *kvs)
{
    if (kvs == NULL) {
        return;
    }
    if (kvs->key != NULL) {
        free(kvs->key);
    }
    // 释放元素列表
    if (kvs->values != NULL) {
        for (int i = 0; i < kvs->values_size; ++i) {
            free(kvs->values[i]);
        }
        free(kvs->values);
    }
    pthread_mutex_destroy(&kvs->mutex);
}

void MR_key_values_push_value(MR_key_values *kvs, const char *value)
{
    if (kvs == NULL || value == NULL) {
        return;
    }
    // 自动扩展
    if (kvs->values_size >= kvs->values_capacity) {
        kvs->values_capacity *= 2;
        char **values = (char **)realloc(kvs->values, sizeof(char *) * kvs->values_capacity);
        if (values == NULL) {
            return;
        }
        kvs->values = values;
    }
    // 分配值字段空间
    MR_KVS_VALUE_AT(*kvs, kvs->values_size) = (char *)malloc(strlen(value) + 1);
    strcpy(MR_KVS_VALUE_AT(*kvs, kvs->values_size), value);
    ++kvs->values_size;
}

void MR_partition_init(MR_partition *partition, int groups_capacity)
{
    if (partition == NULL) {
        return;
    }
    if (groups_capacity > 0) {
        partition->kvs_groups = malloc(sizeof(MR_key_values) * groups_capacity);
    } else {
        partition->kvs_groups = NULL;
        groups_capacity = 0;
    }
    partition->groups_capacity = groups_capacity;
    partition->num_kvs_groups = 0;
    pthread_mutex_init(&partition->mutex, NULL);
}

void MR_partition_destroy(MR_partition *partition)
{
    if (partition == NULL) {
        return;
    }
    if (partition->kvs_groups) {
        for (int i = 0; i < partition->num_kvs_groups; ++i) {
            MR_key_values_destroy(&partition->kvs_groups[i]);
        }
        free(partition->kvs_groups);
    }
    
    pthread_mutex_destroy(&partition->mutex);
}

MR_master *MR_master_create(int num_partitions, int num_pairs)
{
    MR_master *master = malloc(sizeof(MR_master));
    if (master == NULL) {
        return NULL;
    }

    master->num_partitions = num_partitions;
    master->partitions = malloc(sizeof(MR_partition) * num_partitions);
    if (master->partitions == NULL) {
        free(master);
        return NULL;
    }

    for (int i = 0; i < num_partitions; ++i) {
        MR_partition_init(&master->partitions[i], num_pairs);
    }

    return master;
}

void MR_master_destroy(MR_master *master)
{
    if (master == NULL) {
        return;
    }
    if (master->partitions) {
        for (int i = 0; i < master->num_partitions; ++i) {
            MR_partition_destroy(&master->partitions[i]);
        }
        free(master->partitions);
    }
    free(master);
}

// 分区中是否有key addr: 有 NULL: 无
MR_key_values* MR_partition_has_key(MR_partition *partition, const char *key)
{
    for (int i = 0; i < partition->num_kvs_groups; ++i) {
        if (strcmp(MR_PARTITION_KVS_KEY_AT(*partition, i), key) == 0) {
            return &MR_PARTITION_KVS_PAIR_AT(*partition, i);
        }
    }
    return NULL;
}

static MR_key_values* MR_partition_add_key(MR_partition *partition, const char *key)
{
    if (partition == NULL || key == NULL) {
        return NULL;
    }
    MR_key_values *kvs = MR_partition_has_key(partition, key);
    if (kvs) {
        return kvs;
    }
    if (partition->num_kvs_groups >= partition->groups_capacity) {
        int new_capacity = (partition->groups_capacity == 0) ? 4 : partition->groups_capacity * 2;
        kvs = realloc(partition->kvs_groups, sizeof(MR_key_values) * new_capacity);
        if (kvs == NULL) {
            return NULL;
        }
        partition->kvs_groups = kvs;
        partition->groups_capacity = new_capacity;
    }
    kvs = &MR_PARTITION_KVS_PAIR_AT((*partition), partition->num_kvs_groups++);
    MR_key_values_init(kvs, key, 1);
    return kvs;
}

void MR_partition_push(MR_partition *partition, const char *key, const char *value)
{
    if (partition == NULL) {
        return;
    }
    pthread_mutex_lock(&partition->mutex);

    MR_key_values *kvs = MR_partition_add_key(partition, key);
    if (kvs == NULL) {
        pthread_mutex_unlock(&partition->mutex);
        return;
    }
    MR_key_values_push_value(kvs, value);

    pthread_mutex_unlock(&partition->mutex);
}

static int quick_kv_sort_partition(MR_key_values *pairs, int low, int high)
{
    MR_key_values pivot = pairs[low];
    while (low < high)
    {
        // pairs[high].key >= pivot.key
        while (low < high && strcmp(pairs[high].key, pivot.key) >= 0)
            --high;
        pairs[low] = pairs[high];
        // pairs[low].key <= pivot
        while (low < high && strcmp(pairs[low].key, pivot.key) <= 0)
            ++low;
        pairs[high] = pairs[low];
    }
    pairs[low] = pivot;
    return low;
}

void quick_kv_sort(MR_key_values *pairs, int low, int high)
{
    if (low < high)
    {
        int pivot_idx = quick_kv_sort_partition(pairs, low, high);
        quick_kv_sort(pairs, low, pivot_idx - 1);
        quick_kv_sort(pairs, pivot_idx + 1, high);
    }
}
