#ifndef LIBTINYFS_H
#define LIBTINYFS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <limits.h>

#define BLOCK_SIZE 256
#define DEFAULT_DISK_SIZE 10240
#define DEFAULT_DISK_NAME "tinyFSDisk"

typedef int fileDescriptor;
#define INVALID_BLOCK UINT32_MAX

#define RETURN_IF_ERR(c)              do { int _e = (c); if (_e != SUCCESS) return _e; } while (0)
#define IS_BLOCK_USED(bm,n)           (((bm)[(n)>>3] >> ((n)&7)) & 1)
#define SET_BLOCK_USED(bm,n)          ((bm)[(n)>>3] |=  (1 << ((n)&7)))
#define SET_BLOCK_FREE(bm,n)          ((bm)[(n)>>3] &= ~(1 << ((n)&7)))
#define ROLLBACK_MOUNT()              do { closeDisk(mountedDisk); mountedDisk = -1; } while (0)

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint32_t bitmap_block;
    uint32_t root_dir_inode;
    uint32_t fs_size;
    uint16_t checksum;
    uint8_t  padding[BLOCK_SIZE - 1 - 3*sizeof(uint32_t) - sizeof(uint16_t)];
} Superblock;
_Static_assert(sizeof(Superblock) == BLOCK_SIZE, "Superblock size");

typedef struct __attribute__((packed)) {
    uint8_t bitmap[BLOCK_SIZE];
} BitmapBlock;
_Static_assert(sizeof(BitmapBlock) == BLOCK_SIZE, "BitmapBlock size");

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint32_t size;
    uint32_t direct[2];
    uint32_t indirect;
    uint16_t checksum;
    uint8_t  padding[BLOCK_SIZE - 1 - 4*sizeof(uint32_t) - sizeof(uint16_t)];
} Inode;
_Static_assert(sizeof(Inode) == BLOCK_SIZE, "Inode size");

typedef struct __attribute__((packed)) {
    uint8_t  data[BLOCK_SIZE - sizeof(uint16_t)];
    uint16_t checksum;
} Datablock;
_Static_assert(sizeof(Datablock) == BLOCK_SIZE, "Datablock size");

typedef struct __attribute__((packed)) {
    char     name[8];
    uint32_t inode_block;
} DirectoryEntry;

typedef uint32_t Block;
#define DATABLOCK_DATA_SIZE            (BLOCK_SIZE - sizeof(uint16_t))
#define MAX_INDIRECT_BLOCK_POINTERS    (DATABLOCK_DATA_SIZE / sizeof(uint32_t))
#define MAX_DIRECTORY_SIZE             (DATABLOCK_DATA_SIZE / sizeof(DirectoryEntry))

#define MAX_OPEN_FILES 5

typedef enum {
    INODE_TYPE_RO_FILE = 0x01,
    INODE_TYPE_RW_FILE = 0x02
} TYPES;

typedef struct {
    bool     in_use;
    uint32_t inode_block;
    int      offset;
} FileTableEntry;

int tfs_mkfs(char *filename, int nBytes);
int tfs_mount(char *filename);
int tfs_unmount(void);

fileDescriptor tfs_open(char *name);
int tfs_close(fileDescriptor FD);
int tfs_write(fileDescriptor FD, const char *buffer, const int size);
int tfs_delete(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char *buffer);
int tfs_seek(fileDescriptor FD, int offset);
int tfs_writeByte(fileDescriptor FD, int offset, const unsigned char data);

int tfs_makeRO(const char *name);
int tfs_makeRW(const char *name);
int tfs_rename(const char *old_name, const char *new_name);
int tfs_readdir(void);

#endif
