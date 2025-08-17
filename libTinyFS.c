#include "errors.h"
#include "tinyfs_crc.h"
#include "libDisk.h"
#include "libTinyFS_UNIX.h"
#include "crc32.h"
#pragma endregion

// Hard coded such that mountedDisk is always on disk 0 or -1 (unmounted)
// can make block numbers non static and caches of block numbers from superblock
static int mountedDisk = -1;
#define SUPERBLOCK_BLOCK_NUM 0
#define BITMAP_BLOCK_NUM 1
#define ROOT_INODE_BLOCK_NUM 2
#define ROOT_DIR_DATA_BLOCK_NUM 3
static FileTableEntry file_table[MAX_OPEN_FILES];

#pragma region
// doesn't set bitmap
static uint32_t find_free_block(void)
{
    Superblock super_block;
    if (readBlock(mountedDisk, SUPERBLOCK_BLOCK_NUM, &super_block) != SUCCESS)
    {
        printf("Failed to read super_block in find_free_block().\n");
        return INVALID_BLOCK;
    }

    int num_blocks = super_block.fs_size / BLOCK_SIZE;
    BitmapBlock bitmap;
    if (readBlock(mountedDisk, super_block.bitmap_block, &bitmap) != SUCCESS)
    {
        printf("Failed to read bitmap in find_free_block().\n");
        return INVALID_BLOCK;
    }

    for (int i = 0; i < num_blocks; i++)
    {
        if (!IS_BLOCK_USED(bitmap.bitmap, i))
        {
            return (uint32_t)i;
        }
    }
    printf("find_free_block() missed\n");
    return INVALID_BLOCK;
}

static fileDescriptor add_file_descriptor(uint32_t inode_block)
{
    for (int fd = 0; fd < MAX_OPEN_FILES; fd++)
    {
        if (!file_table[fd].in_use)
        {
            file_table[fd].in_use = true;
            file_table[fd].inode_block = inode_block;
            file_table[fd].offset = 0;
            return fd;
        }
    }
    return FS_ERR_FILE_TABLE_FULL;
}

// marks <block> number as used and updates the bitmap accordingly
static void setBlockUsedAndUpdateBitmap(uint32_t block)
{
    if (block == INVALID_BLOCK)
    {
        printf("Attempted to mark INVALID_BLOCK as used in bitmap.");
        return;
    }

    BitmapBlock b;
    readBlock(mountedDisk, BITMAP_BLOCK_NUM, &b);
    SET_BLOCK_USED(b.bitmap, block);
    writeBlock(mountedDisk, BITMAP_BLOCK_NUM, &b);
}

// clears <block> number and updates the bitmap accordingly
static void clearBlockUsedAndUpdateBitmap(uint32_t block)
{
    BitmapBlock b;
    readBlock(mountedDisk, BITMAP_BLOCK_NUM, &b);
    SET_BLOCK_FREE(b.bitmap, block);
    writeBlock(mountedDisk, BITMAP_BLOCK_NUM, &b);
}

// fills <block> with 0x00s
static void zeroBlock(uint32_t block)
{
    char zero[BLOCK_SIZE] = {0};
    int err = writeBlock(mountedDisk, block, zero);
    if (err != SUCCESS)
    {
        printf("zeroBlock recieved error.\n");
        return;
    }
}
#pragma endregion

#pragma region
/* Makes an empty TinyFS file system of size nBytes on an emulated libDisk disk specified by ‘filename’.
This function should use the emulated disk library to open the specified file, and upon success, format the file to be mountable.
This includes initializing all data to 0x00, setting magic numbers, initializing and writing the superblock and other metadata, etc.
Must return a specified success/error code. */
int tfs_mkfs(char *filename, int nBytes)
{
    RETURN_IF_ERR(openDisk(filename, nBytes));
    int disk_to_write = 0;
    mountedDisk = disk_to_write;
    // VALIDATIONS

    int numBlocks = nBytes / BLOCK_SIZE;
    // validate minsize
    // superblock + bitmap block + root_dir(Inode) + root_dir data block (will essentially be the only inode list)
    if (numBlocks <= 3)
    {
        printf("Insufficient nBytes space allocated to tfs_mkfs() for (Min 4 blocks)\n");
        return FS_ERR_INSUFFICIENT_FS_SIZE;
    }

    // VALIDATIONS END

    // wipe disk
    char null_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < numBlocks; i++)
    {
        RETURN_IF_ERR(writeBlock(disk_to_write, i, null_block));
    }

    // write BitmapBlock #1
    BitmapBlock bitmapB;
    memset(bitmapB.bitmap, 0, BLOCK_SIZE);
    SET_BLOCK_USED(bitmapB.bitmap, 0);
    SET_BLOCK_USED(bitmapB.bitmap, 1);
    SET_BLOCK_USED(bitmapB.bitmap, 2);
    SET_BLOCK_USED(bitmapB.bitmap, 3);
    RETURN_IF_ERR(writeBlock(disk_to_write, 1, &bitmapB));
    // PRESET BITMAP for post file system creation ABOVE

    // write root_dir data block #3 (DirectoryEntry {"", UINT32_MAX} to initialize)
    Datablock root_dir_data_block;
    memset(&root_dir_data_block, 0, BLOCK_SIZE);

    int max_entries = sizeof(root_dir_data_block.data) / sizeof(DirectoryEntry);
    DirectoryEntry *entry = (DirectoryEntry *)root_dir_data_block.data;
    for (int i = 0; i < max_entries; i++)
    {
        memset(&entry[i], 0, sizeof(DirectoryEntry)); // zero entire struct
        entry[i].inode_block = INVALID_BLOCK;
    }

    set_datablock_checksum(&root_dir_data_block);
    RETURN_IF_ERR(writeBlock(disk_to_write, 3, &root_dir_data_block));

    // write root_dir Inode #2
    Inode root_dir_inode = {
        .type = INODE_TYPE_RW_FILE,
        .size = 0,
        .direct = {3, INVALID_BLOCK},
        .indirect = INVALID_BLOCK,
        .padding = {0}};
    set_inode_checksum(&root_dir_inode);
    RETURN_IF_ERR(writeBlock(disk_to_write, 2, &root_dir_inode));

    // write Superblock
    Superblock superB = {0};
    superB.type = 0x5A;
    superB.bitmap_block = BITMAP_BLOCK_NUM;
    superB.root_dir_inode = ROOT_INODE_BLOCK_NUM;
    superB.fs_size = nBytes;
    superB.checksum = 0;

    set_superblock_checksum(&superB);
    RETURN_IF_ERR(writeBlock(disk_to_write, 0, &superB));

    //
    // SUPERBLOCK + BITMAP + ROOT_DIR INODE + ROOT_DIR SET UP ATP
    //

    // allocate datablock + indirect block more to root_dir Inode (indepedent)
    uint32_t second_block = find_free_block(); // 2nd direct data block ///ERROR: find_free_block() not finding shit | also the first that uses setBlock
    zeroBlock(second_block);
    setBlockUsedAndUpdateBitmap(second_block);

    uint32_t third_block = find_free_block(); // indirect data block
    if (third_block == INVALID_BLOCK)
    {
        printf("No free block available for indirect block in mkfs.");
        return FS_ERR_BITMAP_FULL;
    }

    zeroBlock(third_block);
    setBlockUsedAndUpdateBitmap(third_block);

    // set up empty indirect block data with checksum
    Datablock buffer_bock = {0};
    // invalidate entries
    Block *indirect_entry = (Block *)buffer_bock.data;
    for (int i = 0; i < MAX_INDIRECT_BLOCK_POINTERS; i++)
    {
        indirect_entry[i] = INVALID_BLOCK;
    }
    set_datablock_checksum(&buffer_bock);
    RETURN_IF_ERR(writeBlock(mountedDisk, third_block, &buffer_bock));
    //

    Inode newInode = {0}; // block for inode
    newInode.type = INODE_TYPE_RO_FILE;
    newInode.size = 0;
    newInode.direct[0] = ROOT_DIR_DATA_BLOCK_NUM; // already made root_dir block
    newInode.direct[1] = second_block;
    newInode.indirect = third_block;
    memset(newInode.padding, 0, sizeof(newInode.padding));
    set_inode_checksum(&newInode);
    RETURN_IF_ERR(writeBlock(mountedDisk, ROOT_INODE_BLOCK_NUM, &newInode));


    closeDisk(disk_to_write);
    mountedDisk = -1;
    return SUCCESS;
}

/* tfs_mount(char *filename) “mounts” a TinyFS file system located within an emulated libDisk disk called ‘filename’.
tfs_unmount(void) “unmounts” the currently mounted file system.
As part of the mount operation, tfs_mount should verify the file system is the correct type.
Only one file system may be mounted at a time.
Use tfs_unmount to cleanly unmount the currently mounted file system.
Must return a specified success/error code. */
int tfs_mount(char *filename)
{
    // all errors will unmount disk
    if (mountedDisk != -1)
    {
        printf("A filesystem is already mounted on the disk\n");
        return FS_ERR_EXISTING_MOUNTED_FS;
    }

    RETURN_IF_ERR(openDisk(filename, 0));
    mountedDisk = 0;

    // validate SUPERBLOCK
    Superblock super_block;
    RETURN_IF_ERR(readBlock(mountedDisk, SUPERBLOCK_BLOCK_NUM, &super_block)); // check FS type
    if (super_block.type != 0x5A)
    {
        ROLLBACK_MOUNT();
        printf("Attempted to mount invalid file system type\n");
        return FS_ERR_WRONG_FS_TYPE;
    }
    // verify superblock checksum
    if (!verify_superblock_checksum(&super_block))
    {
        ROLLBACK_MOUNT();
        printf("Superblock checksum failed.\n");
        return FS_ERR_SB_CHECKSUM_FAILED;
    }
    if (super_block.bitmap_block == INVALID_BLOCK || super_block.root_dir_inode == INVALID_BLOCK)
    {
        ROLLBACK_MOUNT();

        printf("Attempted to mount file system with superblock missing data\n");
        return FS_ERR_MOUNTED_FS_INVALID_SUPERBLOCK;
    }

    // validate root_dir_inode
    Inode root_dir_inode;
    RETURN_IF_ERR(readBlock(mountedDisk, super_block.root_dir_inode, &root_dir_inode));
    if (root_dir_inode.direct[0] == INVALID_BLOCK || root_dir_inode.direct[1] == INVALID_BLOCK || root_dir_inode.indirect == INVALID_BLOCK)
    {
        ROLLBACK_MOUNT();

        printf("Attempted to mount file system with root_dir Inode missing data\n");
        return FS_ERR_MOUNTED_FS_INVALID_ROOT_DIR_INODE;
    }

    // validate root_dir
    Datablock root_dir;
    RETURN_IF_ERR(readBlock(mountedDisk, root_dir_inode.direct[0], &root_dir));

    int max_entries = sizeof(root_dir.data) / sizeof(DirectoryEntry);
    DirectoryEntry *entry = (DirectoryEntry *)root_dir.data;
    for (int i = 0; i < max_entries; i++)
    {
        if (entry[i].inode_block != INVALID_BLOCK &&
            (entry[i].inode_block >= (super_block.fs_size / BLOCK_SIZE)))
        {
            ROLLBACK_MOUNT();
            printf("Attempted to mount file system with root_dir with DirectoryEntry outside of valid range.\n");
            return FS_ERR_MOUNTED_FS_INVALID_ROOT_DIR;
        }
    }

    // validate bitmap_block
    BitmapBlock bitmap_block;
    RETURN_IF_ERR(readBlock(mountedDisk, super_block.bitmap_block, &bitmap_block));
    if (!IS_BLOCK_USED(bitmap_block.bitmap, SUPERBLOCK_BLOCK_NUM) ||
        !IS_BLOCK_USED(bitmap_block.bitmap, ROOT_DIR_DATA_BLOCK_NUM) ||
        !IS_BLOCK_USED(bitmap_block.bitmap, ROOT_INODE_BLOCK_NUM) ||
        !IS_BLOCK_USED(bitmap_block.bitmap, BITMAP_BLOCK_NUM))
    {
        ROLLBACK_MOUNT();

        printf("Attempted to mount file system with Bitmap block missing data\n");
        return FS_ERR_MOUNTED_FS_INVALID_BITMAP;
    }

    return SUCCESS;
}

int tfs_unmount(void)
{
    if (mountedDisk == -1)
        return FS_ERR_NO_FS_MOUNTED;
    RETURN_IF_ERR(closeDisk(mountedDisk));
    mountedDisk = -1;
    return SUCCESS;
}

//
#pragma endregion
// EVERYTHING ABOVE IS CONCERNED WITH DISK OPERATIONS
//=========================================================================================================================================
//  EVERYTHING BELOW IS CONCERNED WITH FILE TABLES AND FILE DESCRIPTORS
#pragma region
//

/* Opens a file for reading and writing on the currently mounted file system.
Creates a dynamic resource table entry for the file (the structure that tracks open files, the internal file pointer, etc.),
and returns a file descriptor (integer) that can be used to reference this file while the filesystem is mounted. */
fileDescriptor tfs_open(char *name)
{
    if (mountedDisk == -1)
    {
        printf("tfs_open() called when no file systems were mounted.\n");
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (name == NULL || strlen(name) > 8)
    {
        return FS_ERR_INVALID_FILENAME;
    }
    // validations end

    Datablock root_dir; // directly reads root_dir inode
    RETURN_IF_ERR(readBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir));

    int max_entries = sizeof(root_dir.data) / sizeof(DirectoryEntry);
    DirectoryEntry *entry = (DirectoryEntry *)root_dir.data;

    Inode thefile = {0};
    int cached_index = -1;
    bool found = false;
    bool missingInode = false;

    for (int i = 0; i < max_entries; i++)
    {
        if (strcmp(entry[i].name, name) == 0)
        {
            cached_index = i;
            // Problem where Inode Directory is INVALID_BLOCK is handled during FS setups
            // if (entry[i].inode_block == INVALID_BLOCK)
            // {
            //     missingInode = true;
            // }
            if (entry[i].inode_block != INVALID_BLOCK)
            {
                RETURN_IF_ERR(readBlock(mountedDisk, entry[i].inode_block, &thefile));
                found = true;
                break;
            }
        }
    }
    if (cached_index == -1)
    { // if not in directory at all, find unused slot in dir
        for (int i = 0; i < max_entries; i++)
        {
            if (entry[i].inode_block == INVALID_BLOCK)
            {
                cached_index = i;
                break;
            }
        }
        if (cached_index == -1)
        {
            printf("tfs_open() tried to craete a new file in a full directory.\n");
            return FS_ERR_DIRECTORY_FULL;
        }
    }

    uint32_t inode_slot;

    if (!found)
    {
        // build an inode (need one block)
        // and allocate 3 datablocks

        inode_slot = find_free_block();
        zeroBlock(inode_slot);
        setBlockUsedAndUpdateBitmap(inode_slot);

        uint32_t first_block = find_free_block(); // 1st direct data block
        zeroBlock(first_block);
        setBlockUsedAndUpdateBitmap(first_block);

        uint32_t second_block = find_free_block(); // 2nd direct data block
        zeroBlock(second_block);
        setBlockUsedAndUpdateBitmap(second_block);

        uint32_t third_block = find_free_block(); // indirect data block
        zeroBlock(third_block);
        setBlockUsedAndUpdateBitmap(third_block);

        // set up empty indirect block data with checksum
        Datablock buffer_bock = {0};
        // invalidate entries
        Block *indirect_entry = (Block *)buffer_bock.data;
        for (int i = 0; i < MAX_INDIRECT_BLOCK_POINTERS; i++)
        {
            indirect_entry[i] = INVALID_BLOCK;
        }
        set_datablock_checksum(&buffer_bock);
        RETURN_IF_ERR(writeBlock(mountedDisk, third_block, &buffer_bock));
        //
        Inode newInode = {0}; // block for inode
        newInode.type = INODE_TYPE_RW_FILE;
        newInode.size = 0;
        newInode.direct[0] = first_block;
        newInode.direct[1] = second_block;
        newInode.indirect = third_block;
        memset(newInode.padding, 0, sizeof(newInode.padding));
        set_inode_checksum(&newInode);

        // push inode
        RETURN_IF_ERR(writeBlock(mountedDisk, inode_slot, &newInode));

        // commit updates to directory
        strncpy(entry[cached_index].name, name, sizeof(entry[cached_index].name));
        entry[cached_index].name[7] = '\0';

        entry[cached_index].inode_block = inode_slot;
    }
    else
    {
        inode_slot = entry[cached_index].inode_block;
    }
    // push updates to directory
    set_datablock_checksum(&root_dir);
    RETURN_IF_ERR(writeBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir));

    fileDescriptor fd = add_file_descriptor(inode_slot);
    // do I need to handle case where fd is null?!!!
    return fd;
}

/* Closes the file and removes dynamic resource table entry */
int tfs_close(fileDescriptor FD)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (FD < 0 || FD >= MAX_OPEN_FILES)
    {
        printf("Attempted to close file descriptor out of range.\n");
        return FS_ERR_ACCESSED_OUT_OF_FILE_TABLE_RANGE;
    }

    if (!file_table[FD].in_use)
    {
        printf("Attempted close on file descriptor not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }

    file_table[FD].in_use = false;
    return SUCCESS;
}

/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire file’s contents,
 to the file described by ‘FD’.
 Sets the file pointer to 0 (the start of file) when done. Returns success/error codes. */
int tfs_write(fileDescriptor FD, const char *buffer, const int size)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (!file_table[FD].in_use)
    {
        printf("Attempted write to a file not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }
    if (size < 0)
    {
        printf("Attempted write with negative size\n");
        return FS_ERR_INVALID_WRITE_SIZE;
    }
    if (size > DATABLOCK_DATA_SIZE * 2 + MAX_INDIRECT_BLOCK_POINTERS * DATABLOCK_DATA_SIZE)
    {                                                              // still supports ==
        printf("Attempted write with unsupportedly large size\n"); // must be crazy big
        return FS_ERR_INVALID_WRITE_SIZE;
    }

    const char *buf_pointer = buffer;

    Inode theinode = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, file_table[FD].inode_block, &theinode));

    if (theinode.type != INODE_TYPE_RW_FILE)
    {
        printf("Invalid permissions to write to file.\n");
        return FS_ERR_INVALID_FILE_PERMISSION;
    }

    int remaining_size = size;
    int chunk_size;

    // clear indirect datablock group | direct datablocks don't need clearing but indirect datablocks need to be freed.
    Datablock indirect_block = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, theinode.indirect, &indirect_block));
    Block *indirect_entry = (Block *)indirect_block.data;
    for (int i = 0; i < MAX_INDIRECT_BLOCK_POINTERS; i++)
    {
        indirect_entry[i] = INVALID_BLOCK;
    }

    if (remaining_size > 0)
    { // first chunk
        chunk_size = remaining_size > DATABLOCK_DATA_SIZE ? DATABLOCK_DATA_SIZE : remaining_size;

        Datablock buffer_block = {0};
        memcpy(buffer_block.data, buf_pointer, chunk_size);
        set_datablock_checksum(&buffer_block);
        RETURN_IF_ERR(writeBlock(mountedDisk, theinode.direct[0], &buffer_block));

        remaining_size -= chunk_size;
        buf_pointer += chunk_size;
    }
    if (remaining_size > 0)
    { // second chunk
        chunk_size = remaining_size > DATABLOCK_DATA_SIZE ? DATABLOCK_DATA_SIZE : remaining_size;

        Datablock buffer_block = {0};
        memcpy(buffer_block.data, buf_pointer, chunk_size);
        set_datablock_checksum(&buffer_block);
        RETURN_IF_ERR(writeBlock(mountedDisk, theinode.direct[1], &buffer_block));

        remaining_size -= chunk_size;
        buf_pointer += chunk_size;
    }
    Block *indirect_pointer = (Block *)indirect_block.data;
    // no need to read existing because gonna overwrite

    int iterator = 0;
    while (remaining_size > 0)
    {
        chunk_size = remaining_size > DATABLOCK_DATA_SIZE ? DATABLOCK_DATA_SIZE : remaining_size;
        indirect_pointer[iterator] = find_free_block();

        Datablock buffer_block = {0};
        memcpy(buffer_block.data, buf_pointer, chunk_size);
        set_datablock_checksum(&buffer_block);
        RETURN_IF_ERR(writeBlock(mountedDisk, indirect_pointer[iterator], &buffer_block));
        setBlockUsedAndUpdateBitmap(indirect_pointer[iterator]);

        remaining_size -= chunk_size;
        buf_pointer += chunk_size;
        iterator++;
    }

    // update indirect block
    set_datablock_checksum(&indirect_block);
    RETURN_IF_ERR(writeBlock(mountedDisk, theinode.indirect, &indirect_block));
    setBlockUsedAndUpdateBitmap(theinode.indirect);

    // update inode
    theinode.size = size;
    set_inode_checksum(&theinode);
    RETURN_IF_ERR(writeBlock(mountedDisk, file_table[FD].inode_block, &theinode));
    setBlockUsedAndUpdateBitmap(file_table[FD].inode_block);

    file_table[FD].offset = 0;
    return SUCCESS;
}

/* deletes a file and marks its blocks as free on disk. */
int tfs_delete(fileDescriptor FD)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (!file_table[FD].in_use)
    {
        printf("Attempted delete on a file not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }

    // prevent root_dir Inode deletion
    if (file_table[FD].inode_block == ROOT_INODE_BLOCK_NUM)
    {
        printf("Refused to delete root directory inode.\n");
        return FS_ERR_PROTECTED_INODE;
    }

    int cached_index = file_table[FD].inode_block;

    Inode theinode = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, file_table[FD].inode_block, &theinode));

    if (theinode.type != INODE_TYPE_RW_FILE)
    {
        printf("Invalid permissions to delete file.\n");
        return FS_ERR_INVALID_FILE_PERMISSION;
    }

    theinode.checksum = 0; // useless but making sure first ins is invalidate checksum
    // MAKE SURE THAT ON MOUNT, MKFS, datablock initialized include inode's direct datablock and indirect datablocks

    // clear direct datablocks
    zeroBlock(theinode.direct[0]);
    clearBlockUsedAndUpdateBitmap(theinode.direct[0]);
    zeroBlock(theinode.direct[1]);
    clearBlockUsedAndUpdateBitmap(theinode.direct[1]);

    // clear indirect datablock group
    Datablock indirect_block = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, theinode.indirect, &indirect_block));
    Block *indirect_entry = (Block *)indirect_block.data;
    for (int i = 0; i < MAX_INDIRECT_BLOCK_POINTERS; i++)
    {
        if (indirect_entry[i] != INVALID_BLOCK)
        {
            zeroBlock(indirect_entry[i]); // clear indirect -> datablocks
            clearBlockUsedAndUpdateBitmap(indirect_entry[i]);
        }
    }
    zeroBlock(theinode.indirect); // clear indirect block itself | no recalc checksum because we're removing entire inode
    clearBlockUsedAndUpdateBitmap(theinode.indirect);

    zeroBlock(file_table[FD].inode_block);
    file_table[FD].in_use = false;
    file_table[FD].inode_block = INVALID_BLOCK;
    file_table[FD].offset = 0;

    return SUCCESS;
}

/* reads one byte from the file and copies it to ‘buffer’, using the current file pointer location and incrementing it by one upon success.
If the file pointer is already at the end of the file then tfs_readByte() should return an error and not increment the file pointer. */
int tfs_readByte(fileDescriptor FD, char *buffer)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (!file_table[FD].in_use)
    {
        printf("Attempted read on a file not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }

    // get Inode block
    Inode theinode = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, file_table[FD].inode_block, &theinode));

    if (file_table[FD].offset >= theinode.size)
    {
        printf("tfs_readbyte() EOF reached.\n");
        return FS_ERR_READ_EOF;
    }

    int datablock_depth = file_table[FD].offset / DATABLOCK_DATA_SIZE;
    int datablock_offset = file_table[FD].offset % DATABLOCK_DATA_SIZE;
    if (datablock_depth < 2)
    { // one of the two data blocks
        char internal_buf[BLOCK_SIZE] = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, theinode.direct[datablock_depth], &internal_buf));
        *buffer = internal_buf[datablock_offset];
    }
    else
    { // inside indirect block -> datablock
        Datablock indirect_block = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, theinode.indirect, &indirect_block));

        Block *indirect_entry = (Block *)indirect_block.data;
        if ((datablock_depth - 2) >= MAX_INDIRECT_BLOCK_POINTERS || indirect_entry[datablock_depth - 2] == INVALID_BLOCK) // check datablock still valid
        {
            return FS_ERR_READ_EOF;
        }
        int datablock_num = indirect_entry[datablock_depth - 2];
        char internal_buf[BLOCK_SIZE] = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, datablock_num, &internal_buf));
        *buffer = internal_buf[datablock_offset];
    }

    file_table[FD].offset++;
    return SUCCESS;
}

/* change the file pointer location to offset (absolute). Returns success/error codes.*/
int tfs_seek(fileDescriptor FD, int offset)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }

    if (!file_table[FD].in_use)
    {
        printf("Attempted read on a file not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }

    Inode buffer_block = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, file_table[FD].inode_block, &buffer_block));

    if (offset < 0 || offset >= buffer_block.size)
    {
        return FS_ERR_INVALID_OFFSET;
    }

    file_table[FD].offset = offset;
    return SUCCESS;
}

int tfs_rename(const char *old_name, const char *new_name)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }

    if (strlen(new_name) > 8 || old_name == NULL || new_name == NULL)
    {
        printf("Attempted to rename with invalid arguments.\n");
        return FS_ERR_INVALID_FILENAME;
    }

    Datablock root_dir = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir)); // statically accesses root_dir
    DirectoryEntry *entries = (DirectoryEntry *)root_dir.data;

    bool found = false;
    for (int i = 0; i < MAX_DIRECTORY_SIZE; i++)
    {
        if (entries[i].inode_block != INVALID_BLOCK && strcmp(entries[i].name, old_name) == 0)
        {
            strncpy(entries[i].name, new_name, sizeof(entries[i].name));
            entries[i].name[7] = '\0';
            found = true;
            break;
        }
    }
    if (!found)
    {
        printf("Filename not found in tfs_rename()\n");
        return FS_ERR_FILE_NOT_FOUND;
    }

    set_datablock_checksum(&root_dir);
    RETURN_IF_ERR(writeBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir));
    return SUCCESS;
}

int tfs_readdir(void) // only statically prints the root dir
{
    Datablock root_dir = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir)); // statically accesses root_dir
    DirectoryEntry *entries = (DirectoryEntry *)root_dir.data;

    printf("Root Directory: ");

    for (int i = 0; i < MAX_DIRECTORY_SIZE; i++)
    {
        if (entries[i].inode_block != INVALID_BLOCK)
        {
            printf(" - %s\n", entries[i].name);
        }
    }

    return SUCCESS;
}

int tfs_makeRO(const char *name)
{ // admin level action
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }

    if (name == NULL || strlen(name) > 8)
    {
        printf("Invalid filename argument in tfs_makeRO().\n");
        return FS_ERR_INVALID_FILENAME;
    }

    Datablock root_dir = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir)); // statically accesses root_dir
    DirectoryEntry *entries = (DirectoryEntry *)root_dir.data;

    bool found = false;
    for (int i = 0; i < MAX_DIRECTORY_SIZE; i++)
    {
        if (entries[i].inode_block != INVALID_BLOCK && strcmp(entries[i].name, name) == 0)
        {
            //
            Inode theinode;
            RETURN_IF_ERR(readBlock(mountedDisk, entries[i].inode_block, &theinode));
            theinode.type = INODE_TYPE_RO_FILE;
            set_inode_checksum(&theinode);
            RETURN_IF_ERR(writeBlock(mountedDisk, entries[i].inode_block, &theinode));
            found = true;
            return SUCCESS;
        }
    }
    printf("Filename not found in tfs_makeRO()\n");
    return FS_ERR_FILE_NOT_FOUND;
}

int tfs_makeRW(const char *name)
{ // admin level action
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }

    if (name == NULL || strlen(name) > 8)
    {
        printf("Invalid filename argument in tfs_makeRW().\n");
        return FS_ERR_INVALID_FILENAME;
    }

    Datablock root_dir = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, ROOT_DIR_DATA_BLOCK_NUM, &root_dir)); // statically accesses root_dir
    DirectoryEntry *entries = (DirectoryEntry *)root_dir.data;

    bool found = false;
    for (int i = 0; i < MAX_DIRECTORY_SIZE; i++)
    {
        if (entries[i].inode_block != INVALID_BLOCK && strcmp(entries[i].name, name) == 0)
        {
            //
            Inode theinode;
            RETURN_IF_ERR(readBlock(mountedDisk, entries[i].inode_block, &theinode));
            theinode.type = INODE_TYPE_RW_FILE;
            set_inode_checksum(&theinode);
            RETURN_IF_ERR(writeBlock(mountedDisk, entries[i].inode_block, &theinode));
            found = true;
            return SUCCESS;
        }
    }
    printf("Filename not found in tfs_makeRW()\n");
    return FS_ERR_FILE_NOT_FOUND;
}

int tfs_writeByte(fileDescriptor FD, int offset, const unsigned char data)
{
    if (mountedDisk == -1)
    {
        return FS_ERR_NO_FS_MOUNTED;
    }
    if (!file_table[FD].in_use)
    {
        printf("Attempted write on a file not in use.\n");
        return FS_ERR_FILE_NOT_IN_USE;
    }

    // get Inode block
    Inode theinode = {0};
    RETURN_IF_ERR(readBlock(mountedDisk, file_table[FD].inode_block, &theinode));

    if (theinode.type != INODE_TYPE_RW_FILE)
    {
        printf("Invalid permissions to write to file.\n");
        return FS_ERR_INVALID_FILE_PERMISSION;
    }

    if (offset >= theinode.size || offset < 0)
    {
        printf("Attempted tfs_writebyte() from EOF.\n");
        return FS_ERR_READ_EOF;
    }

    int datablock_depth = offset / DATABLOCK_DATA_SIZE;
    int datablock_offset = offset % DATABLOCK_DATA_SIZE;

    if (datablock_depth < 2)
    { // one of the two data blocks
        char internal_buf[BLOCK_SIZE] = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, theinode.direct[datablock_depth], &internal_buf));
        internal_buf[datablock_offset] = data;
        set_datablock_checksum((Datablock *)internal_buf);
        RETURN_IF_ERR(writeBlock(mountedDisk, theinode.direct[datablock_depth], &internal_buf));
    }
    else
    { // inside indirect block -> datablock
        Datablock indirect_block = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, theinode.indirect, &indirect_block));

        Block *indirect_entry = (Block *)indirect_block.data;
        if ((datablock_depth - 2) >= MAX_INDIRECT_BLOCK_POINTERS || indirect_entry[datablock_depth - 2] == INVALID_BLOCK) // check datablock still valid
        {
            return FS_ERR_READ_EOF;
        }
        int datablock_num = indirect_entry[datablock_depth - 2];
        char internal_buf[BLOCK_SIZE] = {0};
        RETURN_IF_ERR(readBlock(mountedDisk, datablock_num, &internal_buf));
        internal_buf[datablock_offset] = data;
        set_datablock_checksum((Datablock *)internal_buf);
        RETURN_IF_ERR(writeBlock(mountedDisk, datablock_num, &internal_buf));
    }

    // doesn't auto increment offset like readByte
    return SUCCESS;
}