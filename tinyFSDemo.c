#include <stdio.h>
#include <string.h>
#include "libTinyFS.h"
#include "errors.h"

//demo was updated to showcase error code handling after recording

int main()
{
    const char *diskName = "demo.disk";
    const int diskSize = DEFAULT_DISK_SIZE;

    printf("Creating a new TinyFS file system on '%s'...\n", diskName);
    if (tfs_mkfs((char *)diskName, diskSize) != SUCCESS)
    {
        printf("Error: Failed to create file system (code %d).\n", FS_ERR_INSUFFICIENT_FS_SIZE);
        return -1;
    }

    printf("Mounting file system from '%s'...\n", diskName);
    if (tfs_mount((char *)diskName) != SUCCESS)
    {
        printf("Error: Failed to mount file system (code %d).\n", FS_ERR_WRONG_FS_TYPE);
        return -1;
    }

    printf("Opening new file 'alpha'...\n");
    fileDescriptor fd = tfs_open("alpha");
    if (fd < 0)
    {
        printf("Error: Failed to open/create file 'alpha' (code %d).\n", FS_ERR_DIRECTORY_FULL);
        return -1;
    }

    printf("Writing full content to 'alpha'...\n");
    const char *text = "Hello tinyFS!";
    if (tfs_write(fd, text, strlen(text)) != SUCCESS)
    {
        printf("Error: Failed to write to 'alpha' (code %d).\n", FS_ERR_INVALID_WRITE_SIZE);
        return -1;
    }

    printf("Overwriting one byte at offset 6 with 'X'...\n");
    if (tfs_writeByte(fd, 6, 'X') != SUCCESS)
    {
        printf("Error: Failed to use writeByte on offset (code %d).\n", FS_ERR_READ_EOF);
        return -1;
    }

    printf("Marking file 'alpha' as read-only...\n");
    if (tfs_makeRO("alpha") != SUCCESS)
    {
        printf("Error: Failed to change 'alpha' to read-only (code %d).\n", FS_ERR_FILE_NOT_FOUND);
        return -1;
    }

    printf("Attempting to write to read-only file 'alpha'...\n");
    if (tfs_write(fd, "BLOCKED", 7) != FS_ERR_INVALID_FILE_PERMISSION)
    {
        printf("Error: Expected permission denial, got unexpected behavior.\n");
        return -1;
    }
    else
    {
        printf("Write blocked successfully due to read-only status.\n");
    }

    printf("Attempting to delete read-only file 'alpha'...\n");
    if (tfs_delete(fd) != FS_ERR_INVALID_FILE_PERMISSION)
    {
        printf("Error: Expected delete to fail due to permissions.\n");
        return -1;
    }
    else
    {
        printf("Delete operation correctly blocked for read-only file.\n");
    }

    printf("Reading content back from 'alpha': ");
    char readBuf[64] = {0};
    tfs_seek(fd, 0);
    for (int i = 0; i < strlen(text); i++)
    {
        char byte;
        if (tfs_readByte(fd, &byte) == SUCCESS)
        {
            readBuf[i] = byte;
        }
        else
        {
            printf("[EOF]");
            break;
        }
    }
    printf("\"%s\"\n", readBuf);

    printf("Renaming 'alpha' to 'beta'...\n");
    if (tfs_rename("alpha", "beta") != SUCCESS)
    {
        printf("Error: Rename operation failed (code %d).\n", FS_ERR_FILE_NOT_FOUND);
        return -1;
    }

    printf("Current directory listing:\n");
    tfs_readdir();

    printf("Unmounting the file system...\n");
    if (tfs_unmount() != SUCCESS)
    {
        printf("Error: Failed to unmount file system (code %d).\n", FS_ERR_NO_FS_MOUNTED);
        return -1;
    }

    printf("All operations completed successfully.\n");
    return 0;
}
