#ifndef __PSORT_UTILS_H__
#define __PSORT_UTILS_H__

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

// memory file map
typedef struct {
    int fd;
    char *mbuf;
    off_t size;
} mfilemap;

mfilemap mfilemap_read_open(const char *filename, int oflag,int mmapprot, int mmapflag);

mfilemap mfilemap_write_open(const char *filename, size_t size, int oflag,int mmapprot, int mmapflag);

void mfilemap_close(mfilemap file);

#endif
