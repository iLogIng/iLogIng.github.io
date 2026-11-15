#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, const char** argv)
{
    /* 命令行参数不合适 */
    if (argc > 3) {
        fprintf(stderr, "usage: reverse <input> <output>\n");
        exit(1);
    }

    /* 文件操作  */
    FILE* input_file = stdin;
    FILE* output_file = stdout;
    if (argc >= 2) {
        input_file = fopen(argv[1], "r");
        /* 无法打开 input */
        if (input_file == NULL) {
            fprintf(stderr, "reverse: cannot open file '%s'\n", argv[1]);
            exit(1);
        }
    }
    if (argc == 3) {
        if (strcmp(argv[1], argv[2]) == 0) {
            fprintf(stderr, "reverse: input and output file must differ\n");
            exit(1);
        }
        /* 使用 inode 进行文件比对 */
        struct stat st1, st2;
        if (stat(argv[1], &st1) == 0 && stat(argv[2], &st2) == 0) {
            if (st1.st_ino == st2.st_ino) {
                fprintf(stderr, "reverse: input and output file must differ\n");
                exit(1);
            }
        }
        output_file = fopen(argv[2], "w");
        /* 无法打开 output */
        if (output_file == NULL) {
            fprintf(stderr, "reverse: cannot open file '%s'\n", argv[2]);
            exit(1);
        }
    }

    /* 行缓冲  */
    size_t line_cnt = 0;
    size_t buf_size = 1024;
    char** line_buf = calloc(buf_size, sizeof(char*));
    /* 内存分配错误 */
    if (line_buf == NULL) {
        fprintf(stderr, "reverse: malloc failed\n");
        exit(1);
    }

    /* 读入各行 */
    for (; ; ++line_cnt) {
        /* 行缓冲扩容 */
        if (line_cnt + 1 >= buf_size) {
            buf_size *= 2;
            char** newbuf = realloc(line_buf, sizeof(char*) * buf_size);
            if (newbuf == NULL) {
                fprintf(stderr, "reverse: malloc failed\n");
                exit(1);
            }
            line_buf = newbuf;
        }
        /* 读逻辑 */
        line_buf[line_cnt] = NULL;
        size_t line_size = 0;
        ssize_t ec = getline(&line_buf[line_cnt], &line_size, input_file);
        if (ec == -1) {
            break;
        }
    }

    /* 写逻辑 */
    for (int i = line_cnt - 1; i >= 0; --i) {
        fwrite(line_buf[i], 1, strlen(line_buf[i]), output_file);
    }

    /* 释放资源 */
    for (size_t i = 0; i < line_cnt; ++i) {
        free(line_buf[i]);
    }
    free(line_buf);
    fclose(input_file);
    if (output_file != stdout) {
        fclose(output_file);
    }
    return 0;
}
