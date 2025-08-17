#ifndef LIBTINYFS_H
#define LIBTINYFS_H

#pragma
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
#pragma endregion

#define BLOCK_SIZE 256
#define DEFAULT_DISK_SIZE 10240 
#define DEFAULT_DISK_NAME “tinyFSDisk” 	
typedef int fileDescriptor;

#define INVALID_BLOCK UINT32_MAX

//because checking and returning errors is repetitive*
#define RETURN_IF_ERR(call)        \
    do {                           \
        int err = (call);          \
        if (err != SUCCESS)        \
            return err;            \
    } while (0)

 
#define IS_BLOCK_USED(bitmap, n)    ((bitmap[(n)/8] >> ((n)%8)) & 1)
#define SET_BLOCK_USED(bitmap, n)   (bitmap[(n)/8] |=  (1 << ((n)%8)))
#define SET_BLOCK_FREE(bitmap, n)   (bitmap[(n)/8] &= ~(1 << ((n)%8)))

#define ROLLBACK_MOUNT() do { closeDisk(mountedDisk); mountedDisk = -1; } while(0)

typedef struct {
    uint8_t type;
    uint32_t bitmap_block; //points to a block dedicated to bitmap (usually gonna be block#1)
    uint32_t root_dir_inode; //points to root directory inode block (usually gonna be block #2)
    uint32_t fs_size;
    uint16_t checksum;
    uint8_t padding[BLOCK_SIZE - sizeof(uint8_t) - sizeof(uint32_t)*3 - sizeof(uint16_t)];
} Superblock;

typedef struct {
    uint8_t bitmap[BLOCK_SIZE];
} BitmapBlock;

typedef struct {
    uint8_t type;
    uint32_t size;               // file size in bytes | Directory types can use this as # of entries
    uint32_t direct[2];          // direct data block pointers
    uint32_t indirect;           // block number of an indirect block (contains more pointers)
    uint16_t checksum;
    uint8_t padding[BLOCK_SIZE - sizeof(uint8_t) - sizeof(uint32_t)*3];
} Inode;

typedef struct {
    uint8_t data[BLOCK_SIZE-sizeof(uint16_t)];
    uint16_t checksum;
} Datablock; //template for data blocks 
//used as Directory blocks by accessing as DirectoryEntry*
//used as Indirect blocks by accessing as Block* (uint32_t)*
   //can essentially replace all uses of uint32_t with Block

#define DATABLOCK_DATA_SIZE (BLOCK_SIZE - sizeof(uint16_t))
#define MAX_DIRECTORY_SIZE (DATABLOCK_DATA_SIZE/sizeof(DirectoryEntry))

typedef struct {
    char name[8];
    uint32_t inode_block;
} DirectoryEntry;

typedef uint32_t Block;

#define MAX_INDIRECT_BLOCK_POINTERS ((DATABLOCK_DATA_SIZE)/(sizeof(uint32_t)))

//end block stuff
//================================================================
//starts file stuff

#define MAX_OPEN_FILES 5

typedef enum {
    INODE_TYPE_RO_FILE = 0x01,
    INODE_TYPE_RW_FILE = 0x02,

} TYPES;

typedef struct {
    bool in_use;
    uint32_t inode_block;   // the block number of the file's inode
    int offset;             // current file pointer
} FileTableEntry;

//API
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