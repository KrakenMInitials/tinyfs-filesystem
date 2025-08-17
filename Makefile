CC = gcc
CFLAGS = -g -Wall
TARGET = tinyFSDemo

SRCS = tinyFSDemo.c libTinyFS.c libDisk.c tinyfs_crc.c crc32.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o *.disk