#ifndef __TEST_UTILS_H__
#define __TEST_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <math.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "Record.h"
#include "psort_utils.h"

// 写 Record 类型数据
static inline
size_t fwrite_Record(Record *record, FILE *stream) {
    return fwrite(record->bytes, sizeof(char), RECORD_SIZE, stream);
}

// 随机字符串
void rand_str(char *dest, size_t length) {
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";
    size_t charset_size = sizeof(charset) - 1;
    for (size_t i = 0; i < length; ++i) {
        dest[i] = charset[rand() % charset_size];
    }
    dest[length] = '\0';
}

// 创建随机的符合Record类型格式的文件内容
void create_random_Record_file(const char *filename, RecordKey base, int nrecords, float multiply, unsigned int seed) {
    if (multiply <= 1.f) {
        fprintf(stderr, "multiply must be GREATER than 1.0\n");
        exit(1);
    }
    FILE *stream = fopen(filename, "wb");
    if (stream == NULL) {
        fprintf(stderr, "cannot open/create file %s\n", filename);
        exit(1);
    }
    srand(seed);
    char strbuf[RECORD_VALUE_SIZE + 1];
    int size = (int)ceilf(nrecords * multiply);
    RecordKey *arr = (RecordKey *)malloc(sizeof(RecordKey) * size);
    for (int i = 0; i < size; ++i) {
        arr[i] = i;
    }
    for (int i = 0; i < nrecords; ++i) {
        int j = i + rand() % (size - i);
        RecordKey tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
    for (int i = 0; i < nrecords; ++i) {
        Record record;
        record.fields.key = arr[i] + base;
        rand_str(strbuf, RECORD_VALUE_SIZE);
        set_Record_value(&record, strbuf);
        fwrite_Record(&record, stream);
    }
    fclose(stream);
    free(arr);
}

void check_Record_file_sorted(const char *filename) {
    mfilemap input = mfilemap_read_open(filename, O_RDONLY, PROT_NONE | PROT_READ, MAP_PRIVATE);
    if (input.size % RECORD_SIZE != 0) {
        fprintf(stderr, "%s is not Record Format\n", filename);
        exit(1);
    }
    int sorted = 1;
    int records = input.size / RECORD_SIZE;
    Record *ptr = (Record *)input.mbuf;
    for (int i = 1; i < records; ++i) {
        int pre = i - 1;
        if (ptr[pre].fields.key > ptr[i].fields.key) {
            sorted = 0;
            break;
        }
    }
    if (sorted == 1) {
        fprintf(stdout, "Record file [%s] is sorted\n", filename);
    }
    else {
        fprintf(stderr, "Record file [%s] is not sorted\n", filename);
    }
    mfilemap_close(input);
    return;
}

#endif
