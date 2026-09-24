#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>

#include "thread_pool.h"

// 压缩单元
typedef struct {
    int count;
    char ch;
} zip_elem;

// 压缩缓存
typedef struct {
    char *map_addr;             // 内存映射缓冲
    off_t map_size;             // 内存映射空间
    off_t elem_count;           // 内存元素数量
} map_buf;

// 构建压缩缓存
map_buf *map_buf_create(off_t map_size, int prot, int flags);
// 销毁压缩缓存
void map_buf_destroy(map_buf *mbuf);
// 合并缓存
void merge_zip_buf(map_buf *merge_buf, int len, map_buf **list);

typedef struct {
    map_buf *zip_map;
    off_t offset;
    off_t length;
    map_buf *main_map;
} task_arg;

// 并行压缩任务
void *task(void *arg)
{
    task_arg *targ = (task_arg *)arg;
    char *src = targ->main_map->map_addr + targ->offset;
    size_t srclen = targ->length;
    map_buf *zip_map = targ->zip_map;

    size_t i = 0;
    off_t elem_count = 0;
    char *dst = zip_map->map_addr;

    // 逐拷贝元素进行拷贝
    while (i < srclen) {
        int cnt = 0;
        char ch = src[i];
        while (i < srclen && src[i] == ch) {
            ++i;
            ++cnt;
        }
        memcpy(dst, &cnt, sizeof(int));
        dst += sizeof(int);
        memcpy(dst, &ch, sizeof(char));
        dst += sizeof(char);
        ++elem_count;
    }

    zip_map->elem_count = elem_count;
    return NULL;
}

int main(int argc, char **argv)
{
    const int file_count = argc - 1;
    char **file_list = argv + 1;
    if (file_count == 0) {
        fprintf(stderr, "pzip: file1 [file2 ...]\n");
        exit(1);
    }
    const int threads = get_nprocs();

    // 计算文件列表中所有的文件大小总和
    off_t total_size = 0;
    struct stat st;
    for (int i = 0; i < file_count; ++i) {
        if (stat(file_list[i], &st) != 0) {
            fprintf(stderr, "pzip: cannot stat %s\n", file_list[i]);
            exit(1);
        }
        total_size += st.st_size;
    }

    if (total_size == 0) {
        return 0;
    }

    // 将文件列表中的所有文件读入mmap缓冲
    map_buf *main_map = map_buf_create(total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);
    if (main_map == NULL) {
        fprintf(stderr, "pzip: fail to mmap\n");
        exit(1);
    }
    // 被拷贝字节大小
    off_t copied = 0;
    for (int i = 0; i < file_count; ++i) {
        if (file_list[i] == NULL) {
            continue;
        }
        int fd = open(file_list[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "pzip: cannot open %s\n", file_list[i]);
            continue;
        }
        if (fstat(fd, &st) != 0) {
            fprintf(stderr, "pzip: cannot fstat %s\n", file_list[i]);
            close(fd);
            continue;
        }
        if (st.st_size == 0) {
            close(fd);
            continue;
        }
        // 建立该文件的内存映射
        char *file_map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file_map == MAP_FAILED) {
            fprintf(stderr, "pzip: cannot mmap %s\n", file_list[i]);
            close(fd);
            continue;
        }
        // 合并文件
        memcpy(main_map->map_addr + copied, file_map, st.st_size);
        copied += st.st_size;
        munmap(file_map, st.st_size);
        close(fd);
    }
    main_map->elem_count = copied;

    // 资源分片
    off_t piece_size = total_size / threads + 1;
    map_buf **zip_bufs = malloc(sizeof(map_buf*) * threads);
    task_arg *targs = malloc(sizeof(task_arg) * threads);   // 多线程任务参数
    if (zip_bufs == NULL || targs == NULL) {
        fprintf(stderr, "pzip: malloc failed\n");
        exit(1);
    }
    for (int i = 0; i < threads; ++i) {
        // 最坏情况每个字符都不同，输出为输入的 5 倍 sizeof(int) + sizeof(char)
        zip_bufs[i] = map_buf_create(piece_size * 5, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);
        if (zip_bufs[i] == NULL) {
            fprintf(stderr, "pzip: fail to create zip buffer\n");
            exit(1);
        }
        // 多线程任务参数
        targs[i].zip_map = zip_bufs[i];
        targs[i].offset = i * piece_size;
        if (targs[i].offset >= total_size) {
            targs[i].offset = total_size;
            targs[i].length = 0;
        } else if (targs[i].offset + piece_size > total_size) {
            targs[i].length = total_size - targs[i].offset;
        } else {
            targs[i].length = piece_size;
        }
        targs[i].main_map = main_map;
    }

    // 并行压缩
    thread_pool_t *thread_pool = thread_pool_create(threads);
    if (thread_pool == NULL) {
        fprintf(stderr, "pzip: fail to create thread pool\n");
        exit(1);
    }
    for (int i = 0; i < threads; ++i) {
        if (targs[i].length > 0) {
            thread_pool_add_task(task, &targs[i], thread_pool);
        }
    }

    // 线程收束，资源回收
    thread_pool_destroy(thread_pool);

    // 缓冲合并
    const off_t sig_elem_size = sizeof(int) + sizeof(char);
    off_t total_compressed = 0;
    for (int i = 0; i < threads; ++i) {
        total_compressed += zip_bufs[i]->elem_count * sig_elem_size;
    }
    if (total_compressed == 0) {
        total_compressed = sig_elem_size;
    }
    map_buf *merge_buf = map_buf_create(total_compressed, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);
    if (merge_buf == NULL) {
        fprintf(stderr, "pzip: fail to create merge buffer\n");
        exit(1);
    }
    merge_zip_buf(merge_buf, threads, zip_bufs);

    // 输出到 stdout
    fwrite(merge_buf->map_addr, merge_buf->elem_count * sig_elem_size, 1, stdout);

    // 资源回收
    free(targs);
    for (int i = 0; i < threads; ++i) {
        map_buf_destroy(zip_bufs[i]);
    }
    free(zip_bufs);
    map_buf_destroy(merge_buf);
    map_buf_destroy(main_map);
    return 0;
}

map_buf *map_buf_create(off_t map_size, int prot, int flags)
{
    map_buf *mbuf = malloc(sizeof(map_buf));
    if (mbuf == NULL) {
        return NULL;
    }
    mbuf->map_addr = mmap(NULL, map_size, prot, flags, -1, 0);
    if (mbuf->map_addr == MAP_FAILED) {
        free(mbuf);
        return NULL;
    }
    mbuf->map_size = map_size;
    mbuf->elem_count = 0;
    return mbuf;
}

void map_buf_destroy(map_buf *mbuf)
{
    if (mbuf == NULL) {
        return;
    }
    if (mbuf->map_addr != MAP_FAILED) {
        munmap(mbuf->map_addr, mbuf->map_size);
    }
    free(mbuf);
}

void merge_zip_buf(map_buf *merge_buf, int len, map_buf **list)
{
    const off_t sig_elem_size = sizeof(int) + sizeof(char);

    off_t total_compressed = 0;
    for (int i = 0; i < len; ++i) {
        total_compressed += list[i]->elem_count * sig_elem_size;
    }

    // 按需扩大合并缓冲区
    if (total_compressed > merge_buf->map_size) {
        munmap(merge_buf->map_addr, merge_buf->map_size);
        merge_buf->map_addr = mmap(NULL, total_compressed,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (merge_buf->map_addr == MAP_FAILED) {
            fprintf(stderr, "merge_zip_buf: mmap failed\n");
            exit(1);
        }
        merge_buf->map_size = total_compressed;
    }

    // 合并

    off_t write_offset = 0;
    off_t total_elems = 0;

    for (int i = 0; i < len; ++i) {
        off_t cur_size = list[i]->elem_count * sig_elem_size;
        if (cur_size == 0) {
            continue;
        }

        // 检查与前一个分片的边界：前一片的最后一个元素和当前片的第一个元素
        if (i > 0 && write_offset >= sig_elem_size) {
            int pre_count;
            char pre_ch;
            memcpy(&pre_count, merge_buf->map_addr + write_offset - sig_elem_size, sizeof(int));
            memcpy(&pre_ch, merge_buf->map_addr + write_offset - sig_elem_size + sizeof(int), sizeof(char));

            int cur_count;
            char cur_ch;
            memcpy(&cur_count, list[i]->map_addr, sizeof(int));
            memcpy(&cur_ch, list[i]->map_addr + sizeof(int), sizeof(char));

            // 处理前后单元合并
            if (cur_ch == pre_ch) {
                cur_count += pre_count;
                memcpy(list[i]->map_addr, &cur_count, sizeof(int));
                write_offset -= sig_elem_size;
                total_elems--;
            }
        }

        // 并入merge_buf
        memcpy(merge_buf->map_addr + write_offset, list[i]->map_addr, cur_size);
        write_offset += cur_size;
        total_elems += list[i]->elem_count;
    }

    // 更新元素计数
    merge_buf->elem_count = total_elems;
}
