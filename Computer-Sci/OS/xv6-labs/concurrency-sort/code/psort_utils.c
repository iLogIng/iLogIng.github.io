#include "psort_utils.h"

#define _XOPEN_SOURCE

#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <string.h>

mfilemap mfilemap_read_open(const char *filename, int oflag,int mmapprot, int mmapflag) {
    mfilemap file = {
        .fd = -1,
        .mbuf = MAP_FAILED,
        .size = 0
    };
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("stat");
        return file;
    }
    file.size = st.st_size;
    file.fd = open(filename, O_RDONLY | oflag);
    if (file.fd == -1) {
        perror("open");
        file.size = 0;
        return file;
    }

    file.mbuf = mmap(NULL, file.size,
        PROT_READ | mmapprot, mmapflag,
        file.fd, 0);
    if (file.mbuf == MAP_FAILED) {
        perror("mmap");
        close(file.fd);
        file.fd = -1;
        file.size = 0;
    }

    return file;
}

mfilemap mfilemap_write_open(const char *filename, size_t size, int oflag,int mmapprot, int mmapflag) {
    mfilemap file = {
        .fd = -1,
        .mbuf = MAP_FAILED,
        .size = 0
    };
    if (size == 0) {
        fprintf(stderr, "mfilemap_write_open: size cannot be 0\n");
        return file;
    }
    file.fd = open(filename, O_CREAT | O_RDWR | oflag, 0644);
    if (file.fd == -1) {
        perror("open");
        return file;
    }

    if (ftruncate(file.fd, size) == -1) {
        perror("ftruncate");
        close(file.fd);
        file.fd = -1;
        return file;
    }

    file.size = size;
    file.mbuf = mmap(NULL, file.size,
        PROT_WRITE | mmapprot,
        mmapflag,
        file.fd, 0);
    if (file.mbuf == MAP_FAILED) {
        perror("mmap");
        close(file.fd);
        file.fd = -1;
        file.size = 0;
    }

    return file;
}

void mfilemap_close(mfilemap file) {
    if (file.mbuf != MAP_FAILED && file.mbuf != NULL) {
        if (munmap(file.mbuf, file.size) == -1) {
            perror("munmap");
        }
    }
    if (file.fd != -1) {
        if (close(file.fd) == -1) {
            perror("close");
        }
    }
}
