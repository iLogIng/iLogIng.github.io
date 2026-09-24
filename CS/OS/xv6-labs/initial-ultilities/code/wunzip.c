#include <stdio.h>
#include <stdlib.h>

size_t read_all(FILE* stream);

int main(int argc, const char** argv)
{
    const int file_count = argc - 1;
    const char** file_list = argv + 1;
    /* 无文件 */
    if (file_count == 0) {
        printf("wunzip: file1 [file2 ...]\n");
        exit(1);
    }

    FILE* file = NULL;
    const char* read_mode = "rb";

    for (int i = 0; i < file_count; ++i) {
        const char* file_name = file_list[i];
        file = fopen(file_name, read_mode);
        if (file == NULL) {
            printf("wunzip: cannot open file\n");
            exit(1);
        }
        /* 读取逻辑 */
        read_all(file);
        /* 关闭文件流 */
        fclose(file);
    }

    return 0;
}

size_t read_all(FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    /* 读缓冲 */
    size_t rbs = 1024;
    char* rb = malloc(rbs);

    size_t total = 0;
    int cnt = 0;
    char ch = '\0';

    int idx = 0;
    /* 读出 */
    while (fread(&cnt, sizeof(int), 1, stream) == 1) {
        total += cnt;
        fread(&ch, sizeof(char), 1, stream);
        /* 扩容 */
        if (rbs < idx + cnt) {
            rbs *= 2;
            char* newbuf = realloc(rb, rbs);
            rb = newbuf;
        }
        for (int j = 0; j < cnt; ++j) {
            rb[idx++] = ch;
        }
        if (feof(stream)) {
            break;
        }
    }
    rb[idx] = '\0';
    printf("%s", rb);

    free(rb);
    return total;
}
