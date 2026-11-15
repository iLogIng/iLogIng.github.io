#ifndef __PSORT_H__
#define __PSORT_H__

#include "Record.h"
#include "test_utils.h"
#include "psort_utils.h"

// 排序组
typedef struct {
    Record **list;
    int num_records;
    int iter;
} sort_group;

void sort_group_init(sort_group *group, int num_records) {
    if (group == NULL) {
        return;
    }
    group->list = (Record **)malloc(sizeof(Record *) * num_records);
    if (group->list == NULL) {
        free(group);
        return;
    }
    group->iter = 0;
    group->num_records = num_records;
    return;
}

void sort_group_free(sort_group *group) {
    if (group == NULL) {
        return;
    }
    if (group->list != NULL) {
        free(group->list);
    }
    free(group);
}

int group_qsort_partition(Record **list, int low, int high) {
    Record *tmp = list[low];
    RecordKey pivot = list[low]->fields.key;
    while (low < high) {
        while (low < high && list[high]->fields.key > pivot) {
            --high;
        }
        list[low] = list[high];
        while (low < high && list[low]->fields.key < pivot) {
            ++low;
        }
        list[high] = list[low];
    }
    list[low] = tmp;
    return low;
}

void group_qsort(Record **list, int low, int high) {
    if (low < high) {
        int pivot_idx = group_qsort_partition(list, low, high);
        group_qsort(list, low, pivot_idx - 1);
        group_qsort(list, pivot_idx + 1, high);
    }
}

// 线性归并方法基础，迭代器取用
/*
    for (((Record *)ptr = sort_group_get_next_record(sort_group *, int)) != NULL) {
        memcpy(mmap_ptr, ptr, RECORD_SIZE);
    }
*/
Record *sort_group_get_next_record(sort_group *groups, int num_groups) {
    Record *min_record = NULL;
    int min_idx = -1;

    for (int i = 0; i < num_groups; ++i) {
        if (groups[i].iter < groups[i].num_records) {
            Record *cur = groups[i].list[groups[i].iter];
            if (min_record == NULL || cur->fields.key < min_record->fields.key) {
                min_record = cur;
                min_idx = i;
            }
        }
    }

    if (min_idx >= 0) {
        groups[min_idx].iter++;
    }

    return min_record;
}

#endif