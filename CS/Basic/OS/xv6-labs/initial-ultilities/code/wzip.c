#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

/** 读取文件放入内存
 * args:
 *      - bufptr    char**      缓存区指针
 *      - n         size_t*     缓存区大小
 *      - offset    size_t      偏移量
 *      - stream    FILE*       文件流
 * ret:
 *      - ret       ssize_t     字符数量
*/
ssize_t read_all(char** bufptr, size_t* n, size_t offset, FILE* stream);

ssize_t zipper(char** srcptr, size_t srclen, FILE* stream);

int main(int argc, const char** argv)
{
    const int file_count = argc - 1;
    const char** file_list = argv + 1;
    /* 无文件 */
    if (file_count == 0) {
        printf("wzip: file1 [file2 ...]\n");
        exit(1);
    }

    FILE* file = NULL;
    const char* read_mode = "r";
    size_t total = 0;
    size_t buf_size = 1024;
    char* buf = malloc(buf_size);

    for (int i = 0; i < file_count; ++i) {
        const char* file_name = file_list[i];
        file = fopen(file_name, read_mode);
        if (file == NULL) {
            printf("wzip: cannot open file\n");
            exit(1);
        }
        /* 读取逻辑 */
        size_t nread = read_all(&buf, &buf_size, total, file);
        if (nread == 0) {
            if (ferror(file)) {
                printf("wzip: cannot open file\n");
                exit(1);
            }
        }
        total += nread;
        /* 关闭文件流 */
        fclose(file);
    }

    /* 压缩逻辑 */
#if 0
    int i = 0;
    while (i < total) {
        char ch = buf[i];
        int cnt = 0;
        while (ch == buf[i] && i < total) {
            ++i;
            ++cnt;
        }
        fwrite(&cnt, sizeof(int), 1, stdout);
        fwrite(&ch, 1, 1, stdout);
    }
#endif
    zipper(&buf, total, stdout);

    free(buf);
    return 0;
}

ssize_t read_all(char** bufptr, size_t* n, size_t offset, FILE* stream)
{
    if (bufptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    /* 初始化内存 */
    if (*bufptr == NULL) {
        *n = 256;
        *bufptr = malloc(256);
        if (*bufptr == NULL) {
            return -1;
        }
    }

    ssize_t i = offset;
    int ch;
    while ((ch = getc(stream)) != EOF) {
        /* 扩容 */
        if (i + 1 >= *n) {
            *n *= 2;
            char* newbuf = realloc(*bufptr, *n);
            if (newbuf == NULL) {
                return offset;
            }
            *bufptr = newbuf;
        }
        /* 读入 */
        (*bufptr)[i++] = ch;
    }

    if (ferror(stream) || (i == 0 && feof(stream))) {
        return -1;
    }

    (*bufptr)[i] = '\0';
    return i - offset;
}

ssize_t zipper(char** srcptr, size_t srclen, FILE* stream)
{
    /* 检查 */
    if (srcptr == NULL) {
        return -1;
    }
    /* 压缩文件大小 */
    ssize_t zsize = 0;
    char* src = *srcptr;
    /* 整数转字符串缓冲 */
    size_t i = 0;
    while (i < srclen) {
        int cnt = 0;
        char ch = src[i];
        while (src[i] == ch && i < srclen) {
            ++i;
            ++cnt;
        }
        /* 整数转字符串 */
        fwrite(&cnt, sizeof(int), 1, stream);
        fwrite(&ch, 1, 1, stream);
        zsize += sizeof(int) + 1;
    }
    return zsize;
}
