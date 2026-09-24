#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

__ssize_t my_getline(char** lineptr, size_t* n, FILE* stream);

int substr_match(const char* sub, const char* str);

int main(int argc, const char** argv)
{
    const int file_count = argc - 2;
    const char** file_list = argv + 2;
    /* 搜索词 */
    const char* target_str = argv[1];
    /* 无参数调用 */
    if (argc == 1) {
        printf("wgrep: searchterm [file ...]\n");
        exit(1);
    }

    /* 针对文件 */
    size_t buf_size = 1000;
    char* buf = NULL;

    /* 仅有查找词 */
    if (file_count == 0) {
        while (my_getline(&buf, &buf_size, stdin) >= 0) {
            if (substr_match(target_str, buf)) {
                printf("%s", buf);
            }
        }
        free(buf);
        return 0;
    }

    FILE* file = NULL;
    char* open_mode = "r";
    /* 读取文件 */
    for (int i = 0; i < file_count; ++i) {
        const char* file_name = file_list[i];
        file = fopen(file_name, open_mode);
        if (file == NULL) {
            printf("wgrep: cannot open file\n");
            exit(1);
        }
        /* 行处理 */
        while (my_getline(&buf, &buf_size, file) >= 0) {
            if (substr_match(target_str, buf)) {
                printf("%s", buf);
            }
        }
        fclose(file);
    }

    free(buf);
    return 0;
}

__ssize_t my_getline(char** lineptr, size_t* n, FILE* stream)
{
    if (lineptr == NULL || n == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (*lineptr == NULL || *n == 0) {
        *n = 256;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) {
            return -1;
        }
    }
    
    size_t i = 0;
    int c;
    /* 逐个字符读取，并扩展缓冲区 */
    while((c = fgetc(stream)) != EOF) {
        if (i + 2 >= *n) {
            if (*n > (__SIZE_MAX__ >> 1)) {
                errno = ENOMEM;
                return -1;
            }
            *n *= 2;
            char* newbuf = realloc(*lineptr, *n);
            if (newbuf == NULL) {
                return -1;
            }
            *lineptr = newbuf;
        }
        (*lineptr)[i++] = (char)c;
        if (c == '\n') {
            break;
        }
    }

    if (ferror(stream) || (i == 0 && feof(stream))) {
        return -1;
    }
    (*lineptr)[i] = '\0';
    return i;
}

int substr_match(const char* sub, const char* str)
{
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] != sub[0]) {
            continue;
        }
        int j = 0;
        for (j = 0; sub[j] != '\0'; ++j) {
            int str_pos = i + j;
            if (str[str_pos] == 0 || sub[j] != str[str_pos]) {
                break;
            }
        }
        if (sub[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

