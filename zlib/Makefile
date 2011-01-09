CC=gcc

OBJS = adler32.o compress.o crc32.o gzio.o uncompr.o deflate.o trees.o \
       zutil.o inflate.o infback.o inftrees.o inffast.o

all: $(OBJS)

%.o:%.c
	$(CC) -O3 -Wall -c $<
