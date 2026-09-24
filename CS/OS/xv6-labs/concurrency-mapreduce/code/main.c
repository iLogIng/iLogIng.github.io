#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include "mapreduce.h"

void test_wc_Map(char *file_name);

void test_wc_Reduce(char *key, Getter get_next, int partition_number);

int main(int argc, char *argv[])
{
    // MR_Run(argc, argv, map_proc, threads, reduce_proc, threads, MR_DefaultHashPartition);
    MR_Run(argc, argv, test_wc_Map, 10, test_wc_Reduce, 10, MR_DefaultHashPartition);

    return 0;
}

// 进行单词计数
void test_wc_Map(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    assert(fp != NULL);

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            // Map 函数内调用 MR_Emit 送出 中间键值对
            // 此时的 (token, "1") 已经是中间键值对了
            MR_Emit(token, "1");
        }
    }
    free(line);
    fclose(fp);
}

// 输出键出现的次数
void test_wc_Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    while ((value = get_next(key, partition_number)) != NULL)
        count++;
    if (count != 0)
        printf("%s %d\n", key, count);
}