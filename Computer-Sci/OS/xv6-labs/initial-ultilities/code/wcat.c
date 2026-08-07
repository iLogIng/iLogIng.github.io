#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* const* argv)
{
    const int file_count = argc - 1;
    char* const* file_list = argv + 1;
    const char* open_mode = "r";
    if (file_count == 0) {
        exit(0);
    }

    int buf_size = 1000;
    char* rbuffer = malloc(buf_size);
    /* 按列表打开文件 */
    for (int i = 0; i < file_count; ++i) {
        const char* file_name = file_list[i];
        FILE* file = fopen(file_name, open_mode);
        /* 文件打开状况检查 */
        if (file == NULL) {
            printf("wcat: cannot open file\n");
            exit(1);
        }
        /* 读取文件 */
        while(fgets(rbuffer, buf_size, file)) {
            printf("%s", rbuffer);
        }
        fclose(file);
        /* printf("\n"); cat 程序无该行*/
    }

    free(rbuffer);
    return 0;
}