# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall
OBJS = wserver.o wclient.o request.o io_helper.o FIFO_buffer.o SFF_buffer.o scheduler.o thread_pool.o

LINK = -lpthread

.SUFFIXES: .c .o 

all: wserver wclient spin.cgi

wserver: wserver.o request.o io_helper.o FIFO_buffer.o SFF_buffer.o scheduler.o thread_pool.o
	$(CC) $(CFLAGS) $(LINK) -o wserver wserver.o request.o io_helper.o FIFO_buffer.o SFF_buffer.o scheduler.o thread_pool.o

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) -o wclient wclient.o io_helper.o

spin.cgi: spin.c
	$(CC) $(CFLAGS) -o spin.cgi spin.c

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) wserver wclient spin.cgi
