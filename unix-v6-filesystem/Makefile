CC      = gcc
CFLAGS  = -g -O2 -std=gnu11 -Wall -Wextra -Werror
OBJS    = diskimg.o inode.o file.o directory.o pathname.o

.PHONY: all test clean

all: mkfs_v6 diskimageaccess runtests

mkfs_v6: mkfs_v6.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

diskimageaccess: diskimageaccess.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

runtests: runtests.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Build a fresh image and run the assertion suite.
test: all
	./mkfs_v6 sample.img
	./runtests sample.img

clean:
	rm -f *.o mkfs_v6 diskimageaccess runtests sample.img
