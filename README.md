# TinyFS

TinyFS is a toy filesystem implementation built for my CSC 453 assignment.  
It’s a flat, block-based filesystem that mimics a few POSIX-style operations, with some realistic extensions inspired by ZFS principles.

I took the opportunity to implement what I learned about ZFS in setting up my own homelab running TrueNAS (at the time) during a class project to develop a filesystem. It includes features and considerations sourced from the ZFSHandbook.com and trimed down features that fit the scope of the class project. The mock implementation consists of block checksums, hybrid data block indexing in inodes, comprehensive error codes, and reasonable fault-tolerance. Github readme includes an unpolished demo indicating fault tolerance and checksuming. To date, this is one the projects that went unbelievable well and smoothly due to the, unknowingly, test-driven development on each layer of the file system and functions taken. I wrote sample test codes to make sure the intended results of each function or sequence of functions were achieved and worked on functions thoroughly one at a time, abstracting away the complexities completed ones entirely and trusting they were not the issue if an error comes up. 
One of the main challenges was in distinguishing directories and files in the 'datablock's while leaving the last 16 bytes for the checksum value and making sure actual data values (file entries for directories and content for files) were isolated from the checksum. I found a workaround by having a constant MAX_DIRECTORY_SIZE  and MAX_DIRECTORY_SIZE which took into account the 16 bytes and were used to compare against content sizes to write on write. Due to the freedom of C, I also simply accessed the data values for directory file types using struct DirectoryEntry pointers.


The goal here isn’t production-readiness but to exercise mechanical sympathy with how filesystems actually tick: superblocks, inodes, data blocks, free block management, etc.

[Watch Demo Video (Google Drive)](https://drive.google.com/file/d/1rVkdr50zKSI2eMQdF26UdKRi6SBIcHcW/view?usp=sharing)


---

## ✨ Features

- **Block-based design** (256B blocks, 40 blocks by default = 10KB “disk”).
- **Superblock** at block 0:
  - Magic number (`0x5A`)
  - Pointer to root inode
  - Bitmap-based free block management
  - Checksum for integrity
- **Inodes**:
  - File size tracking
  - Two direct block pointers + one indirect pointer (multi-block file support)
  - CRC32 checksums for integrity
- **Root directory**:
  - Flat namespace only (no subdirectories)
  - Supports file names up to 8 alphanumeric characters
  - Directory entries store `name → inode` mappings
- **Data blocks**:
  - Fixed 256B
  - Copy-on-write semantics (never overwrite in place)
  - CRC32 checksum per block
- **Free blocks**:
  - Managed via bitmap stored in the superblock
- **Error codes**:
  - Unified negative integers for all FS + disk errors (see `errors.h`).

---

## 🔧 API Overview

TinyFS exposes a set of pseudo system calls (C functions) to work with the filesystem:

- `tfs_mkfs(filename, nBytes)` → Format a new TinyFS on a disk file.
- `tfs_mount(filename)` / `tfs_unmount()` → Attach/detach a filesystem.
- `tfs_open(name)` / `tfs_close(fd)` → Open and close files.
- `tfs_write(fd, buffer, size)` → Write an entire buffer into a file.
- `tfs_readByte(fd, buffer)` → Read one byte at a time.
- `tfs_seek(fd, offset)` → Move file pointer.
- `tfs_delete(fd)` → Delete file and free blocks.
- `tfs_writeByte(fd, offset, byte)` → Overwrite a single byte.
- `tfs_makeRO(name)` / `tfs_makeRW(name)` → Toggle permissions.
- `tfs_rename(old, new)` → Rename a file.
- `tfs_readdir()` → Print directory contents.

See [`tinyFSDemo.c`](./tinyFSDemo.c) for a runnable example showcasing these operations.

---

## 🚀 Build & Run

```bash
# Build the project
make

# Run the demo
./tinyFSDemo
```
---
## Development Blog and Notes

Names of all partners: Moe Aung (Solo)

An explanation of how well TinyFS implementation works:
- implemented Open-source checksum CRC32 code and validations 
- has checksum for validations across core structures and decent coverage
- ZFS style-indexing of Inodes
- Handles a robust amount of error codes and segfault proofing which can all be handled on the Demo using the libTinyFS interface
- Bitmap Block based block management
- complexity from being only able to write to data segment of datablocks (because of attached checksum)
- anything related to clearing entries, wipes the entire blocks clean (defensive programming) and not just invalidate their pointers
- attempted atomicity (rollback, all or nothing, error recoveries in mounting) (at least on mounting) 


Any limitations or bugs the file system has:
- Each inode have 2 direct data block pointer and 1 indirect block pointer including 4 byte checksum in each so holds up to 16,896 bytes (16.5 KB) of data. It is fixed though and neither allocates less or more.
- My FS is limited to one level of hierachy aka root_directory
- FileTable is lost when disk is unmounted

An explanation of additional functionality areas and how they work:
- Read-only and writeByte support:
tfs_makeRO(char *name) 
tfs_makeRW(char *name) 
tfs_writeByte(fileDescriptor FD, int offset, unsigned char data)
- Directory listing and renaming
tfs_rename(...) 
tfs_readdir(...)


Additional blabber:

I am modelling my File System after ZFS

ZFS Superblock
- has 8 bit type 0x5A
- block \# of bitmap block (feature)
- block \# of root directory (asgn req)
- file system size
- checksum (at the end)
- paddings to complete BLOCK_SIZE

BitmapBlock
- a block was dedicated to keeping track of free blocks using bitmap

ZFS Inodes
- file size storage
- two direct pointers to data blocks
- one pointer to an indirect blocks

ZFS Data blocks
- has a segment for data
- has a checksum at the end of block | I use CRC32 (4 bytes)

Indirect Blocks and Directories
- Datablocks' data segment are accessed as DirectoryEntry* (for Directories) or Block* (for Indirect Blocks)
- has checksum (at the end)

- unintialized block pointers set to INVALID_BLOCK|UINT32_MAX (because I use uint instead of int and cannot use -1 to invalidate)

Notes:
Universal:
- ZFS Checksum in Datablocks, Superblock, Inode blocks
- ZFS Inodes with 2 direct data block pointers and one indirect block pointer

Universal (None ZFS):

FILE TABLES
- inactive file entries are kept and when a new file is opened, the file table is iterated through and the first inactive entry it sees is replaced.

potential improvements:
- error codes could've been grouped by functions but I was working iteratively on the functions so they should indirectly be so
- multiple hierachy levels | given my code, it shouldn't be neccessarily too hard. I already have Directories anyways. 
