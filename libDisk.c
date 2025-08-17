#include "errors.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "libDisk.h"
#pragma endregion

typedef struct {
    int fd;         // File descriptor
    int sizeBytes;  // Total usable disk size (nBytes)
    int sizeBlocks;  // Usually constant
    bool isActive;
} Disk;

#define BLOCK_SIZE 256
#define DISK_ARRAY_SIZE 1

static Disk disks_array[DISK_ARRAY_SIZE]; //statically capped to 1 disk

static int find_free_index(){
    for (int i = 0; i < DISK_ARRAY_SIZE; i++) {
    if (!disks_array[i].isActive)
        return i;
    }
    return -1;
}

static bool file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}


int openDisk(char *filename, int nBytes){

    if (nBytes % BLOCK_SIZE != 0 || nBytes < 0){
        perror("Invalid nBytes argument in openDisk()\n");
        return DISK_ERR_OPEN_DISK_BAD_ALIGNMENT;
    }

     //check nBytes not 0
     //if nBytes not 0, check existing file:
        //update existing file
    if (nBytes > 0){

        int file = open(filename, O_RDWR | O_CREAT, 0666);
        //catch fopen() fail
        if (file < 0) {
            perror("open() failed");
            return SYSTEM_ERROR;
        }       
        if (ftruncate(file, nBytes) < 0) {
            perror("ftruncate() failed");
            return SYSTEM_ERROR;
        }        

        int free_index = find_free_index();
        disks_array[free_index].fd = file;
        disks_array[free_index].isActive = true;
        disks_array[free_index].sizeBytes = nBytes;
        disks_array[free_index].sizeBlocks = nBytes/BLOCK_SIZE;

        return SUCCESS;
    } 

    else { //nBytes == 0 
         //filename is an existing disk, possibly not created by the program
        int file = open(filename, O_RDWR);
        if (file < 0) {
            perror("open() failed");
            return SYSTEM_ERROR;
        }       
        
        struct stat filestat;
        if (fstat(file, &filestat) < 0) {
            perror("fstat() failed");
            close(file);
            return SYSTEM_ERROR;
        }
        
        if (filestat.st_size == 0 || filestat.st_size % BLOCK_SIZE != 0) { //check if existing file("disk") is actually valid
            close(file);
            return DISK_ERR_OPEN_DISK_BAD_ALIGNMENT;
        }

        int free_index = find_free_index();
        if (free_index < 0) {
            close(file);
            return DISK_ERR_DISK_ARRAY_FULL;
        }
        disks_array[free_index].fd = file;
        disks_array[free_index].isActive = true;
        disks_array[free_index].sizeBytes = filestat.st_size;
        disks_array[free_index].sizeBlocks = filestat.st_size / BLOCK_SIZE;

        return SUCCESS;
    }
}

 //reads into <block> from Block bNum
int readBlock(int disk, int bNum, void* block){
    if (!disks_array[disk].isActive){
        return DISK_ERR_DISK_INACTIVE;
    }
    Disk* thedisk = &disks_array[disk];

    if (bNum < 0 || bNum >= thedisk->sizeBlocks){
        perror("Tried to access outside of block space\n");
        return DISK_ERR_DISK_ACCESS_DENIED;
    }
    if (lseek(thedisk->fd, bNum * BLOCK_SIZE, SEEK_SET) < 0) {
        perror("lseek() failed in readBlock()");
        return DISK_ERR_DISK_ACCESS_FAILED;
    }
    if (read(thedisk->fd, block, BLOCK_SIZE) != BLOCK_SIZE){
        perror("read() in readBlock() didn't read BLOCK_SIZE \n");
        return DISK_ERR_DISK_ACCESS_FAILED;
    }

    return SUCCESS;
}

 //writes from <block> into Block bNum
int writeBlock(int disk, int bNum, void* block){
    if (!disks_array[disk].isActive){
        return DISK_ERR_DISK_INACTIVE;
    }
    Disk* thedisk = &disks_array[disk];
    if (bNum < 0 || bNum >= thedisk->sizeBlocks){
        perror("Tried to access outside of block space\n");
        return DISK_ERR_DISK_ACCESS_DENIED;
    }
    if (lseek(thedisk->fd, bNum * BLOCK_SIZE, SEEK_SET) < 0) {
        perror("lseek() failed in readBlock()");
        return DISK_ERR_DISK_ACCESS_FAILED;
    }
    if (write(thedisk->fd, block, BLOCK_SIZE) != BLOCK_SIZE){
        perror("write() in writeBlock() missed bytes\n");
        return DISK_ERR_DISK_ACCESS_FAILED;
    }

    return SUCCESS;
}

int closeDisk(int disk){ //assignment specifics this return void?
    Disk* thedisk = &disks_array[disk];
    if (!thedisk->isActive){
        perror("Disk already inactive");
        return DISK_ERR_DISK_INACTIVE;
    }
    
    thedisk->isActive = false;
    thedisk->sizeBytes = -1;
    thedisk->sizeBlocks = -1;
    if (close(thedisk->fd) < 0) {
        perror("close() failed in closeDisk()");
        return SYSTEM_ERROR;
    }
    return SUCCESS;
    //doesn't really clear from array (outside of requirement scope)
    //might need for increasing disk size beyond 1
}